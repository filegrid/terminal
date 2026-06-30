# Windows Terminal (Portable Geek Edition)

![](images/all.png)

## Project information

- **Version**: 0.0.1
- **Author**: tommy (tommy.yxd@gmail.com)

## Project background

This project focuses on the pain points that show up in geek vibe coding workflows:

- Fast context recovery when working across multiple projects and modules at the same time
- Easier input for command-line agent tools, with an experience closer to native Windows components
- Protection against losing a coding window because of accidental operations

## Documentation

- **Chinese overview**: [README-cn.md](README-cn.md)
- **Build notes**: [ext\README.md](ext/README.md)

## Feature overview

### Workspace

Workspaces help manage terminals for different development needs within the same scenario.

For workspaces:

- Click the workspace name in the upper-left corner to enter workspace maintenance and use the unified management entry

![](images/menu.png)

![](images/manage.png)

- One workspace can only be opened in one terminal window at a time
- Locking a workspace helps prevent accidental changes; once unlocked, normal workspace management becomes available again

For each terminal node inside a workspace:

- The session can be restored from its source (a profile from the base Windows Terminal configuration or an SSH config entry), startup directory, and startup command
- For newly opened terminal windows, the app infers the startup directory and command to reduce configuration work; the management entry can still be used for correction. To improve inference accuracy, using Tab or similar shortcuts for command completion is not recommended

### Bottom input panel

- **Independent control**: each terminal node can enable or disable its own bottom input panel, and it is hidden by default
- **Auto-sizing input height**: the input box starts at one line and grows with the actual number of input lines
- **Send**: `Ctrl+Enter` sends the current input to the active terminal and restores the newline behavior expected by that terminal
- **Draft cache**: input content is cached per panel, and it remains available after reopening the workspace so you can continue editing

### Portable distribution

1. Delivered and run as a single-file build.
2. English and Chinese release variants are currently supported.

### SSH configuration

1. Connection information can be loaded from SSH config files.
2. SSH entries are de-duplicated against existing terminal connections.
