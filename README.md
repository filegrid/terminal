# Windows Terminal Geek Portable Edition

Windows Terminal Geek Portable Edition is a portable, workspace-focused build of Windows Terminal. It restores related terminal sessions together, keeps workspace configuration separate from the normal terminal settings, and provides project-oriented terminal and web-tool workflows.

![Workspace overview](res/images/all.png)

## Highlights

- **Portable distribution**: run the single-file release without an MSIX installation. `bin\Start-WindowsTerminalPortable.cmd` uses its own directory as the portable root.
- **Workspace management**: group profiles, SSH connections, startup directories, and startup commands into reusable workspaces. Lock a workspace to prevent accidental edits.
- **Command subtabs**: place up to five terminal commands or WebView tools inside one workspace node without adding more top-level tabs.
- **Integrated web tools**: embed local or intranet pages such as code-server/codev directly beside a project's terminals.
- **Input panel and recovery**: send multi-line terminal input with `Ctrl+Enter`, retain unfinished text, and recover SSH-aware startup state, including Windows hosts reached through `ssh -t`.

## Quick start

Launch `bin\Start-WindowsTerminalPortable.cmd`. To store the extracted payload, settings, and workspaces in a chosen directory, launch with:

```powershell
WindowsTerminalPortable.exe --portable-root <directory>
```

Open workspace management from the workspace name at the upper left. Create or select a node, choose its profile and startup directory, then configure its command windows. See the command-subtabs guide for the complete workflow and the codev example.

## Documentation

- [Workspace node command subtabs guide](docs/usage/eng/workspace-node-command-subtabs.md)
- [Release notes](docs/usage/eng/release-notes.md)
- [Chinese command-subtabs guide](docs/usage/cn/workspace-node-command-subtabs.md)
- [Chinese release notes](docs/usage/cn/release-notes.md)
- [Build guide](README-build.md)
- [Design and implementation notes](docs/)

## Building

Read the [Build guide](README-build.md) before compiling, testing, packaging, or reporting a build result. For Host, Settings, tab, XAML, or terminal-runtime changes, validate the deliverable with:

```powershell
cmake --build build --target full
```

Individual library, DLL, executable, or internal Ninja targets are diagnostic checks only and may leave `bin/` with an older payload.

## Repository layout

- `src/`: workspace core logic and Terminal-facing glue.
- `res/`: workspace resources.
- `tools/`: build and resource-generation tools.
- `microsoft/`: Windows Terminal source and checked-in dependencies.
- `docs/usage/eng/` and `docs/usage/cn/`: English and Chinese user documentation.
- `bin/`: portable release artifacts.

## License and upstream project

This repository is based on the Windows Terminal source tree. Refer to the upstream files under `microsoft/` for license notices and component-level attributions.
