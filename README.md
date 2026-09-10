# GeekTerminal

GeekTerminal is a workspace-focused terminal for Windows, built for AI-assisted and vibe-coding workflows. It restores the working context for all of your projects when you reopen a workspace.

Releases are available in two forms: a portable EXE for direct use, and an MSI installer for a standard Windows installation.

![Workspace tabs and command subtabs](res/images/workspace.png)

## One workspace, many project workflows

A workspace can represent one large project or a complete development environment such as development, staging, or production. It is a named collection of project nodes. A common practical pattern is to use one first-level node tab per subproject, with that subproject defining a set of workflow windows:

```text
Workspace
├─ API service (node / top-level tab)
│  ├─ AI coding agent
│  ├─ Source code shell
│  ├─ Debug page
│  └─ Local client
└─ Web app (node / top-level tab)
   ├─ AI coding agent
   ├─ Source code shell
   ├─ Debug page
   └─ Local client
```

Use one or more agent command windows alongside the shell for source code and WebView tabs for debugging pages or local clients. Reopening the workspace restores each node's configured project workflow without filling the top-level tab bar.

![Workspace and node configuration](res/images/node.png)

- **Workspace and project nodes** — organize multiple subprojects in one workspace. Each node keeps its profile, startup directory, SSH connection, icons, colors, and startup commands.
- **Project workflow subtabs** — combine multiple AI agents, source-code shells, debugging pages, and local clients inside one node. Put their selector at the top left, top right, or bottom right.
- **Two release options** — use the portable EXE directly, or install the MSI for a standard Windows setup.

## Terminals, local tools, and debugging together

Use one node per subproject: add agent terminals and source-code shells, then add the debug dashboards and local clients that support the work. A workflow window can start a terminal command or show an already-running local service URL.

![Embedded debugging tool](res/images/debug.png)

![Embedded code workspace](res/images/code.png)

Web windows use the system-installed WebView Runtime. The application does not download, install, package, or select its version. If a page cannot render correctly, install or update the Runtime from the [official WebView page](https://developer.microsoft.com/en-us/microsoft-edge/webview2/), then restart the application.

## Quick start

1. Choose a release: run the portable EXE directly, or install the MSI package.
2. Select the workspace name at the upper left to open workspace management.
3. Create a workspace and one node for each subproject, then select a terminal profile and startup directory.
4. Add AI-agent and source-code command windows with `+`, then add debugging pages or local clients with the globe icon and their complete URLs.
5. Save and reopen the workspace to create the configured sessions.

## Documentation

- [User guide](docs/usage/eng/getting-started.md)
- [FAQ](docs/usage/eng/faq.md)
- [Workspace node command subtabs guide](docs/usage/eng/workspace-node-command-subtabs.md)
- [Release notes](docs/usage/eng/release-notes.md)
- [中文使用说明](docs/usage/cn/getting-started.md)
- [常见问题](docs/usage/cn/faq.md)
- [工作区节点命令子 Tab 使用说明](docs/usage/cn/workspace-node-command-subtabs.md)
- [中文发行说明](docs/usage/cn/release-notes.md)
- [Build guide](README-build.md)

## License and upstream project

This repository is based on the Windows Terminal source tree. Refer to the upstream files under `microsoft/` for license notices and component-level attributions.
