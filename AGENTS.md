# CI costs money: never trigger it without being asked

**Rule (set by Duarte, 2026-10-12): do not start, re-run or re-enable any GitHub Actions workflow in these repositories unless Duarte explicitly asks for it in the current conversation.** CI minutes are billed to him and he does not track the usage.

What this means in practice, for any AI assistant or script working here:

- The engine repository (`Peixinho/Pyros3D`) has its four workflows (Linux, macOS, Web, Windows) **disabled on purpose** (`gh workflow disable`). Do not enable them. With them disabled, a `git push` builds nothing - that is intended.
- Do not run `gh workflow run`, `gh run rerun`, `gh workflow enable`, or push a tag (`v*` tags start the game repository's Release workflow) unless asked.
- If a task needs a CI artifact (a Windows build, a release package), **stop and ask first**. Say what would run and on which repository; wait for a yes. One yes covers that one run, not later ones.
- When asked for a build: enable only the workflow needed, start it once, and disable it again when it finishes (`gh workflow enable Windows`, `gh workflow run Windows`, then `gh workflow disable Windows`).
- Batch changes: never push several times in a row while a paid build is wanted - one build with everything in it.
- If you find a run in progress that nobody asked for, cancel it (`gh run cancel <id>`) and say so.
