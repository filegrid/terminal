# Release Notes

[中文](../cn/release-notes.md)

## Added: workspace node command subtabs

Workspace nodes can now contain up to five command windows. The windows remain within one first-level node tab and are switched in the node content area using command subtabs (or a vertical icon strip), keeping related tools out of the top-level tab row.

- Set a name, icon, and startup command for each command window, then drag to reorder them.
- Use the **Tab** display mode, with command subtabs positioned at the top-left, top-right, or bottom-right.
- Terminal command windows start independently and are not restarted when switching subtabs.
- Add a WebView command window to embed a local or intranet web tool, such as code-server/codev at `http://127.0.0.1:8080`.
- Adding a WebView automatically selects Tab mode; WebView windows are not supported in split mode.
- The workspace command configuration format is now v3, while reading remains compatible with legacy single-command and v2 configurations.

For instructions and a codev integration example, see the [workspace node command subtabs guide](workspace-node-command-subtabs.md).
