# Build Guide

This repository builds the portable Windows Terminal product through CMake and
Ninja. The supported build root is the repository root, and final portable
artifacts are written to `bin/`.

## Prerequisites

- Windows 10 or later
- Visual Studio C++ build tools and the Windows SDK required by the repository
- .NET SDK version pinned by `CMakeLists.txt`
- The checked-in `microsoft/` dependency tree

Use a regular PowerShell session. The CMake configuration locates the compiler
and Windows SDK tools; a Developer Command Prompt is not required.

## Configure and build

```powershell
cmake -S . -B .\build
cmake --build .\build
```

The default build follows the incremental extension packaging path. To rebuild
the complete product graph and regenerate the portable artifact, run:

```powershell
cmake --build .\build --config Release --target full
```

For a debug build, use:

```powershell
cmake --build .\build --config Debug --target full
```

## Outputs

The portable single-file outputs are written to `bin/`:

- `WindowsTerminalPortableGeekEdition_System_<version>_<arch>.exe`
- `WindowsTerminalPortableGeekEdition_System_Debug_<version>_<arch>.exe`

Only these final files should be used to validate portable startup behavior.
Intermediate executables, `wt.exe`, `OpenConsole.exe`, MSIX files, and files
under `microsoft/bin/` are build inputs or diagnostics rather than the final
portable distribution.

## Source layout

- `src/core/` contains business, persistence, state, and launch-planning logic.
- `src/glue/` contains Terminal, WinRT, UI, runtime logging, and packaging
  integration.
- `src/generated/` contains generated source headers, including the build
  version header produced from `VERSION`.
- `res/` contains workspace resources.
- `tools/` contains resource-generation and build helper scripts.

Keep glue code limited to adaptation and wiring. New business rules, state
machines, persistence policies, and runtime decisions belong in `src/core/`
unless an explicit integration requirement makes that impossible.

## Troubleshooting

- Reconfigure after moving source paths: `cmake -S . -B .\build`.
- Run `full` when changes affect product DLLs, generated metadata, packaging,
  or the portable launcher.
- Verify the final executable in `bin/` rather than testing an intermediate
  binary from the build tree.
