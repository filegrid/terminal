# Workspace Node Command Subtabs Guide

A workspace node can contain multiple command windows while remaining a single first-level tab. Select the node, then switch its command windows from the second-level tabs in the content area or from the right-side icon strip. This keeps related terminals, services, and web tools out of the top-level tab row.

## Configure command subtabs

1. Open **Workspace Management** from the top-right drop-down menu, then choose the workspace and node in the left navigation.
2. In **Command windows**, choose **Add command window** and enter its name and startup command. Leave the command empty to start only the node profile.
3. Choose **Add WebView window**, then enter its name and complete URL, for example `http://127.0.0.1:8080`.
4. Choose an icon for each item and adjust its order.
5. Under **Multi-window display**, choose tabs or side-by-side. A node containing WebView uses tabs.
6. Choose top-left, top-right, or bottom-right under **Tab position**, then select **Save** at the bottom of the management tab. Reopen the workspace to create sessions from the new configuration.

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
