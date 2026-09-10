# FAQ

## Why did saving not change the open workspace?

Save updates the workspace definition; it does not restart sessions that are already open. Reopen the workspace from the top-left workspace selector to create the new nodes, command windows, and layout.

## Why can’t I edit or remove something?

The workspace may be locked. Unlock it before editing in **Workspace Management**. Every node must retain at least one command window and can have at most five.

## Why can’t a WebView be side by side with terminals?

A WebView uses a browser host and cannot be placed in Terminal’s native split-pane tree. Nodes with WebViews use the subtab layout. Put the service address in the WebView URL and start the service yourself before opening the workspace.

## A web window is blank or does not load

Web windows require the system WebView2 Runtime. Install or update the [WebView2 Runtime](https://developer.microsoft.com/en-us/microsoft-edge/webview2/), then restart GeekTerminal. Also check that the URL includes a protocol such as `http://` and that the local service is listening on that port.

## Why did every terminal disappear when I closed a node?

A top-level node tab is the container for all command windows in that node. Closing it closes every contained window. Use the node's subtabs when you only want to switch windows.

## Where are the configuration files?

Select **Open configuration directory** at the bottom of Workspace Management navigation. Use the management page for normal changes; after manual file edits, reopen the workspace.
