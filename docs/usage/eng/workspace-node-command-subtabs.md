# Workspace Node Command Subtabs Guide

A workspace node can contain multiple command windows while remaining a single first-level tab. Select the node, then switch its command windows from the second-level tabs in the content area or from the right-side icon strip. This keeps related terminals, services, and web tools out of the top-level tab row.

## Prerequisites

- The workspace must be unlocked and in edit mode.
- The node must have a valid terminal profile. Terminal command windows inherit its profile, startup directory, and title policy.
- WebView windows use the system-installed default WebView Runtime. The application does not download, install, package, or select its version or path. If a web page does not render correctly, install or update the Runtime from the [official page](https://developer.microsoft.com/en-us/microsoft-edge/webview2/), then restart the application.

## Configure command subtabs

1. Open workspace management from the workspace name at the upper left, choose the workspace and node, then enter edit mode.
2. In **Command windows**, select `+` to add a terminal command window and enter its name and startup command. Leave the command empty to start only the node's profile.
3. Select the globe icon to add a WebView window, then enter its name and complete URL, for example `http://127.0.0.1:8080`.
4. Select an item's left icon to set its subtab icon; drag items to change their order.
5. With two or more command windows, select **Tab** under **Multi-window display**. Adding a WebView selects Tab mode automatically; WebView windows are not supported in split mode.
6. Choose top-left (icon and text), top-right (vertical icons), or bottom-right (bottom-aligned vertical icons) for the subtab position. Save and reopen the workspace to create the new sessions.

Each node supports one to five command windows. Closing a first-level node tab closes every window it contains; switching subtabs does not restart an already created terminal session.

## codev example

If you use codev, start it from the project directory after installing it according to its own documentation:

```bash
cd /path/to/your-project
codev
```

It prints an address such as `http://127.0.0.1:8080`. Configure the following windows in one workspace node and select **Tab** display mode:

| Order | Name | Type | Command or URL |
| --- | --- | --- | --- |
| 1 | Development terminal | Terminal | Empty, or `wsl` / your project startup command |
| 2 | VS Code Web | WebView | `http://127.0.0.1:8080` |
| 3 | Dev Server | Terminal | `npm run dev` |

Multiple projects use consecutive ports such as `8080` and `8081`; enter the URL that `codev` prints for each node. Manage instances with:

```bash
cd /path/to/your-project
codev stop       # removes this project's container, not project files or shared extensions
codev list       # lists recorded instances
```

By default, `codev` uses `--auth none` and binds to `0.0.0.0`; use it only on the local machine or a trusted network. From Windows WebView on WSL2, prefer `http://127.0.0.1:<port>`. WebView displays the page only and does not start `codev`; run `codev` first.
