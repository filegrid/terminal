# User Guide

[中文](../cn/getting-started.md)

## Workspace management

Choose **Workspace Management** from the top-right drop-down menu. It opens a dedicated Workspace Management tab.

The workspace selector at the top left opens saved workspaces. Create and edit definitions in the Workspace Management tab.

## Create a workspace

1. Choose **New workspace** in the left-navigation footer of the Workspace Management tab.
2. Choose a blank workspace or create a copy of an existing workspace.
3. In **General**, set the workspace name, description, icon, and color.
4. Choose **Add node**, then select the new node.
5. Set the node name, profile, startup directory, startup command or script, and first-level-tab visibility.
6. Select **Save** at the bottom.

Saving writes the workspace definition. Open the workspace from the top-left workspace selector to create node sessions from the new configuration; editing does not restart an open workspace.

## Nodes and command windows

A node is a first-level tab and normally represents one subproject. It can contain command windows for AI agents, source-code shells, development services, and debugging pages without turning each into a top-level tab.

On the node page, use **Command windows** to add terminal or WebView windows. Terminal windows use the node profile and startup directory; WebView windows require a complete URL. See [Workspace node command windows](workspace-node-command-subtabs.md).

## FAQ

See the [FAQ](faq.md).
