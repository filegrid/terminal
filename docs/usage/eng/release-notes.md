# Release Notes

[中文](../cn/release-notes.md)

## Improved: workspace WebView stability and interaction

- Improved the native web host used by workspace web commands and workspace management.
- Fixed the web rendering layer covering top-right/bottom-right command icons and workspace close controls; icon spacing has returned to its original layout.
- Completed mouse-input forwarding, focus handoff, and close lifecycle handling so a terminal pane does not take focus back from a web page.
- Workspace node-tab titles can no longer enter transient rename mode by double-click or command; titles are managed by the workspace configuration.

## Added: workspace node command subtabs

Workspace nodes can now contain up to five command windows. The windows remain within one first-level node tab and are switched in the node content area using command subtabs (or a vertical icon strip), keeping related tools out of the top-level tab row.

- Set a name, icon, and startup command for each command window, then drag to reorder them.
- Use the **Tab** display mode, with command subtabs positioned at the top-left, top-right, or bottom-right.
- Terminal command windows start independently and are not restarted when switching subtabs.
- Add a WebView command window to embed a local or intranet web tool, such as code-server/codev at `http://127.0.0.1:8080`.
- Adding a WebView automatically selects Tab mode; WebView windows are not supported in split mode.
- Existing workspace command configurations remain supported.

For instructions and a codev integration example, see the [workspace node command subtabs guide](workspace-node-command-subtabs.md).
