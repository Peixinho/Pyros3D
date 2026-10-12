# CI: what is free, what is not, and what needs asking

Set by Duarte on 2026-10-12, after checking the billing page.

**Engine repository (`Peixinho/Pyros3D`, public): CI runs on every push and is free** - GitHub does not bill Actions on public repositories with standard runners. Its four workflows (Linux, macOS, Web, Windows) are enabled. Keep it free:

- Never change a workflow to a "larger runner" (those are billed even on public repositories). Standard ones only: `ubuntu-*`, `windows-*`, `macos-*`.
- Never make the repository private without telling Duarte that every build would then be billed (Windows 2x, macOS 10x).
- Still batch changes: one push with everything beats several pushes that cancel each other's builds.

**Game repository (`Peixinho/erangel`, private): CI only on demand.** Its minutes come out of a free allowance of 2,000 a month. Do not start its Release workflow (`gh workflow run`, `gh run rerun`) or push a `v*` tag unless Duarte asks for it in the current conversation. If a task needs a release package, stop and ask first; one yes covers one run.

**Any other private repository: the same as the game repository - ask first.**

If you find a run on a private repository that nobody asked for, cancel it (`gh run cancel <id>`) and say so.
