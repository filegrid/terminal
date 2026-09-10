# Workspace Nodes and Command Windows

A node is a workspace top-level tab. Command windows are terminals or WebViews inside the node. A node supports one to five command windows.

## Add and edit windows

1. Open **Workspace Management** from the top-right drop-down menu, then open the workspace and target node in the left navigation.
2. In **Command windows**, click `+` to add a terminal window or the globe icon to add a WebView window.
3. Name every window. For a terminal window, enter its startup command; leave it empty to start the terminal with the node profile and startup directory. A WebView needs a complete URL, such as `http://127.0.0.1:8080`.
4. Choose an icon, drag windows into order, or remove them. Removal is available only while the node still has another window.
5. Save the workspace and reopen it to create sessions from the edited definition.

The node profile and startup directory are shared by its terminal windows. A WebView does not execute a terminal command: it loads its URL directly. Start any local service yourself before opening the workspace.

## Choose a window layout

With two or more windows, **Multi-window display** offers:

- **Side by side**: show multiple terminals at once and drag dividers to adjust their proportions.
- **Tab**: show one window at a time; place the subtab strip at top left, top right, or bottom right.

A node containing a WebView uses the tab layout. A WebView cannot be hosted in Terminal's native split-pane tree. Switching subtabs does not restart an existing terminal session.

## Runtime behavior

Opening a workspace creates nodes from its saved definition. Closing a node's top-level tab closes all of its terminals and WebViews; switching a node or subtab does not close them.

A practical pattern is to keep an AI Agent, source terminal, development-service terminal, and service debugging page in one node. You may also keep one ordinary terminal window; nodes do not require a multi-window layout.
