# User Guide

[中文](../cn/getting-started.md)

## Workspaces, nodes, and windows

A workspace is a reusable development-session definition, not a required project structure. It can represent one large project or a development, staging, or production environment.

A node normally represents a subproject. One node can group a workflow of windows: several AI Agent terminals, source-code terminals, development services, debugging pages, or web tools. This is a useful working pattern, not a constraint.

Command windows belong to a node rather than the top-level tab row. A node therefore stays one top-level tab while its related sessions are switched or shown side by side inside it.

## Open and create a workspace

- The workspace selector at the top left opens a saved workspace.
- **Workspace Management** in the top-right drop-down menu creates, edits, copies, deletes, and saves workspaces.

To create one:

1. Select **New workspace** at the bottom of the Workspace Management navigation, then choose a blank workspace or an existing workspace as a template.
2. Under **General**, set its name, description, icon, and color.
3. Under **Node defaults**, set the default profile, startup directory, input panel, tab title, and top-level-tab visibility for new nodes.
4. Click `+` on the workspace navigation item to add a blank node or copy an existing node as a template.
5. Open the node and set its name, profile, startup directory, icon, and command windows.
6. Select **Save**, then open the workspace from the top-left selector.

Save updates the definition only; editing does not restart a workspace that is already open. Reopen it to create sessions from the new definition.

## Edit and use workspaces

The workspace **General** page lets you drag visible nodes into top-level-tab order. On a node page, configure its profile and startup directory; each command window can have its own name, icon, order, and be removed.

Lock a workspace when you want to prevent accidental structural changes. Unlock it before editing. Closing a node's top-level tab closes every command window inside that node.

For terminal, WebView, tab, and split-layout details, see [Workspace nodes and command windows](workspace-node-command-subtabs.md). For the Docker/code-server example and WebView setup, see [VS Code Web with Docker](code-server-docker.md).

## Configuration files

Use **Open configuration directory** at the bottom of the Workspace Management navigation to open the folder containing workspace definitions. Prefer the management page for ordinary editing; after manual changes, reopen the workspace to create sessions from the new configuration.

## FAQ

See the [FAQ](faq.md).
