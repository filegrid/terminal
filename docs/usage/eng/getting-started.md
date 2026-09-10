# User Guide

GeekTerminal uses a workspace to represent either one large project or a complete environment such as development, staging, or production. A common practical pattern is to organize subprojects as nodes in the first-level tab row, with each subproject defining workflow windows such as AI-agent command windows, source-code shells, debugging pages, and local client pages. This is a recommended organization pattern, not a requirement.

[中文](../cn/getting-started.md)

For installation or runtime dependencies, see the [FAQ](faq.md).

## Start the application

Run the downloaded executable directly.

## Language

The application automatically uses the system language. Restart the application after changing the system UI language.

## Create and edit a workspace

1. Select the workspace name at the upper left to open workspace management.
2. Create a workspace or select an existing one.
3. Create one node for each subproject, choose its terminal profile, and set its startup directory and name.
4. Enter edit mode to change the configuration. Save and reopen the workspace for the new session configuration to take effect.
5. Lock a workspace to prevent accidental edits. Unlock it before editing again.

Terminal windows in a node inherit that node's selected profile and startup directory. Closing a node's first-level tab closes all command windows in that node.

## Configure command windows

Each node supports one to five command windows. Treat these as the subproject workflow: one or more AI-agent terminals, a source-code shell, and web windows for debugging pages or local clients.

- Select `+` to add a terminal command window. Enter a name and startup command; leave the command empty to start only the node profile.
- Select the globe icon to add a web window. Enter a name and a complete URL, such as `http://127.0.0.1:8080`.
- Select the icon at the left of a command window to change its icon, and drag entries to reorder them.
- With two or more windows, choose a multi-window display mode. Web windows use Tab display mode and do not support split display.
- Put subtabs at the top left, top right, or bottom right. Switching subtabs does not restart a terminal session that has already been created.

See the [Workspace Node Command Subtabs Guide](workspace-node-command-subtabs.md) for complete command-subtab, WebView, and codev examples.

## Web windows

Web windows use the system-installed default WebView Runtime. The application does not download, install, package, or select its version or path.

If a page does not render or behaves unexpectedly, install or update the Runtime from the [official WebView Runtime page](https://developer.microsoft.com/en-us/microsoft-edge/webview2/), then restart the application. A web window displays a page only; start a local service such as codev separately before opening its URL.

## Common checks

- Cannot edit a workspace: make sure it is unlocked and that you are in edit mode.
- A new configuration is not visible: save it, then reopen the workspace to create the new sessions.
- A web window is blank or cannot load: verify the URL and local service in a regular browser first, then check that the WebView Runtime is current.
- To review changes, read the localized [release notes](release-notes.md).
