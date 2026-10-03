# Multiplayer field test: two machines, two networks

Everything in the networking stack is tested on loopback, in CI, on one
machine. Loopback cannot show three things, and this is the checklist for
them:

1. a **built game** from one machine running on another (other OS, other GPU);
2. **real latency and loss** between two networks;
3. **NAT traversal** - whether two home routers let a hosted game through.

Nothing here has been run yet. Each step says what to expect and what a
failure looks like, so a result can be reported as "step 4 failed with …".

## 0. What you need

- Machine **A** (has the editor) and machine **B** (any of Windows, Linux,
  macOS), ideally on **different networks** (B on a phone hotspot is enough).
- For step 5 only: a machine with a public UDP port (any small cloud VM).

## 1. Build the game for both machines

On A, in the editor: **File > Build Game**.

- *Dedicated server*: set a password; leave the port at 47400.
- *Platform*: "This machine" for A's copy. For B's copy pick B's platform -
  that needs its player template once:

  ```
  tools/fetch_templates.sh windows      # or linux / macos; needs `gh`, signed in
  ```

  The dialog shows `Template: ~/.pyros3d/templates/<platform>` when it is there.
- Build twice, into two folders (`game-A`, `game-B`); copy `game-B` to B.

Expect on B: `PyrosPlayer` starts and shows the startup scene on its own.
Failure looks like: a missing library on launch (report its name - CI's
package step is what should have bundled it), or a black window (report
B's GPU and the last lines of the terminal output).

## 2. Pin the server's key

On the machine that will host the dedicated server (say A):

```
cd game-A && ./PyrosServer --print-key
```

It prints 64 hex characters and creates `server.key` (keep it; never ship
it). Paste the printed key into Build Game's **Server public key** and
build both folders again - `server.key` survives a rebuild. From now on
clients refuse any server that is not this one.

Expect: joining with `--server-key <some other 64 hex>` prints
`the server refused the connection - server key mismatch`.

## 3. Same network first

```
A:  cd game-A && ./PyrosServer --stats 5
B:  ./PyrosPlayer --connect <A's LAN address> --password <password>
```

Expect on A, every 5 s: `1 players, N objects, frame avg … ms, … kbit/s out
per player`. A wrong password prints `… refused the connection - wrong
password` on B. A firewall prompt on A (Windows, macOS) must be allowed:
the server listens on **UDP 47400**.

## 4. Two networks, port forwarded

Put B on the other network. On A's router forward **UDP 47400** to A, then:

```
B:  ./PyrosPlayer --connect <A's public address> --password <password>
```

What to look at:

- the server's `--stats` line: round trip and kbit/s per player are now
  real numbers. For reference, loopback with 100 players in one place is
  about 270 kbit/s per player.
- pull B's network for ~10 seconds (reconnect grace is 30 s by default):
  the server should log a drop, B should come back by itself and own its
  objects again. For longer than the grace: B is gone for good, as intended.

## 5. Two networks, no port forwarding (NAT traversal)

Remove the port forward. On the public machine (copy any built game folder
there, or just `PyrosServer` and the engine library):

```
VM: ./PyrosServer --rendezvous-service 47400
```

Open **UDP 47400** in the VM's firewall. Then one player hosts from home
and the other joins by name:

```
A:  ./PyrosPlayer --host --rendezvous <VM address> --session "field test" --password <password>
B:  ./PyrosPlayer        --rendezvous <VM address> --session "field test" --password <password>
```

Expect on the VM: `1 host(s) announced`. Expect on B: `joining field test`
and then the game.

Failure modes, and what they mean:

| What B sees | Meaning |
|---|---|
| fails within a second | the service knows no such name: A never reached the VM (firewall, wrong address) |
| fails after ~5 s | B cannot reach the VM |
| hangs ~30 s, then fails | both reached the VM but the routers did not let the packets through: one of them is a *symmetric* NAT (common on mobile carriers and corporate networks). This design cannot cross those - it would need a relay. Report which networks were used. |

Two machines behind the **same** router cannot meet by name (the router's
public address does not loop back on most routers): use the LAN address.

The dedicated server can announce a name too
(`./PyrosServer --rendezvous <VM> --session <name>`), which lets players
find a server without knowing its address.

## 6. Many players against the real server

From any machine with the repository built:

```
c++ -std=c++17 -O2 -DPYROS_NETWORKING -I include -I src/Pyros3D/Ext/box3d/include \
    tools/bench/net_bots.cpp -o net_bots -L build -lPyrosEngine -Wl,-rpath,$PWD/build
./net_bots --game <a built game folder> --connect <server address> --password <password> --count 100 --seconds 60
```

It needs the game's server script to spawn each joining peer an object it
owns. Watch the server's `--stats` line while it runs; over a real link
the number that matters is `frame … worst` staying under the tick (33 ms
at 30 Hz) and `kbit/s out per player` times the player count fitting the
server's uplink.

Reference, from one machine (Apple Silicon laptop, loopback, 2026-10-03):
a built 8 x 8 km streamed world (1024 cells), `PyrosServer` at 30 Hz,
100 bots walking, one `PyrosPlayer` flying at 300 m/s.

| Layout | Server frame avg / worst | Out per player | Server memory |
|---|---|---|---|
| players spread over the map | 2.6-2.9 ms / 7 ms | 8 kbit/s | 212 MB, with ~930 of the 1024 cells loaded |
| all within 200 m | 3.7 ms / 11 ms | 262 kbit/s | under 100 MB |

The first run of the spread layout used 5.7 GB: the server was building
every terrain tile's render geometry (6.8 MB a cell). It now builds none
(`HeightfieldMesh::SetHeadless`), keeping the heights physics stands on.
When all 100 leave at once the worst frame is 25 ms and memory falls back
to 29 MB.

The flying client held the 60 Hz cap (sampled frames 16.7 ms average,
22 ms worst) at about 850 MB. The players in that test had no mesh,
no physics body and no animation, and the scene no light: these are the
costs of the world and the network, not of a finished game.

