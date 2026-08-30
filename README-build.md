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
```

After configuration, choose exactly one supported package target:

```powershell
# Core / Glue-only changes that do not alter the Host payload
cmake --build .\build --config Release --target ext

# Any Host, XAML, tab, packaging, or final-product change
cmake --build .\build --config Release --target full
```

## Mandatory architecture and build rules

### 1. Do not change the three-layer source structure

The product source structure has exactly these three layers:

- `src/core/`: pure bottom-layer rules, validation, persistence planning, and data transforms.
- `src/glue/`: Ext loading, ABI adaptation, runtime integration, and other bridge code.
- `microsoft/`: the original Windows Terminal Host, including pages, XAML, Settings, controls, tabs, and application lifecycle.

Do not add a fourth application source layer or move code across these three
layers merely to make a build pass. In particular, `ext` is a packaging/build
target, not a separate `src/ext` source layer. Ext-facing implementation belongs
in `src/glue/` unless it is genuinely pure Core logic.

### 2. Keep `microsoft/` changes focused on the page contract

Use the Host first to establish the requested page effect, interaction flow, and
acceptance criteria. Keep that Host work limited to page layout, commands, and
the host-side contract; do not move Core rules or Glue implementation into
`TerminalPage` or other Host files for a perceived build benefit. Preserve the
existing page lifecycle unless the requested page effect explicitly requires a
change.

### 3. Implement and test from the page inward

The required sequence is:

1. Host page effect and its narrow Host-side contract in `microsoft/`
2. Pure rules, validation, persistence planning, and data transforms in `src/core/`
3. Ext-facing adaptation and runtime/ABI integration in `src/glue/`

Use the Host step to define and verify the visible behavior before implementing
the supporting rule. Then implement the rule in Core, and finally connect it
through Glue. For every step, complete the code change, the correct package
build, and its targeted test before moving to the next step. The Host must not
absorb Core policy or Glue runtime concerns merely because it is implemented
first.

### 4. Select the package target from the changed layer

| Change type | Location | Required package/test | When `full` is mandatory |
| --- | --- | --- | --- |
| Pure Core implementation | `src/core/` | Start with `ext` only when the changed code is consumed exclusively by Ext/Glue; otherwise use `full`. | Core is statically or publicly consumed by Host/Settings, or the final Host payload changes. |
| Ext-facing logic or Glue runtime | `src/glue/` | `ext` package and relevant ABI/lifetime tests. | The change alters a Host-facing contract, generated metadata, or Host payload. |
| Host page, Host runtime, Settings, tabs, XAML | `microsoft/` | `full` package and final portable validation. | Always. |
| Shared public contract, CMake target graph, packaging tool | `src/contracts/`, `CMakeLists.txt`, `tools/portable/` | Reconfigure first; validate the affected package path. | Use `full` whenever Host or final portable assembly may be affected. |

`ext` is the fast extension package: it reuses the existing Host payload and
updates Ext/Glue. It is invalid for a Host-affecting change because it can leave
the portable package with an old Host. `full` rebuilds and packages the complete
product.

Only `ext` and `full` are supported developer build targets. Both produce a
portable artifact in `bin/`. Do **not** invoke individual compilation, library,
DLL, executable, MSIX, or packaging-subtargets (for example `TerminalAppLib`,
`WindowsTerminal`, `Ext`, or `ninja` targets) as a delivery or validation path.
Those targets can compile successfully while leaving `bin/` unchanged or with an
older Host payload. A source change is not ready to run until either `ext` or
`full` has completed successfully and emitted a new file under `bin/`.

Do not add new targets, PCHs, unity builds, caches, or module splits merely for
cleaner structure. Keep such a build optimization only when a same-machine,
same-configuration measurement shows fewer affected compilation targets or a
repeatable reduction in the correct package build time.

Use `ext` only for the eligible Ext/Glue path. To rebuild the complete product
graph and regenerate the portable artifact, run:

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
