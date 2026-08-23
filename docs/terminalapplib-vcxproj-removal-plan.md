# TerminalAppLib.vcxproj Removal Plan

## Goal

Remove `microsoft/src/cascadia/TerminalApp/TerminalAppLib.vcxproj` from the repository without breaking the `cmake + ninja` build or leaving the Visual Studio / MSBuild project graph in a half-broken state.

This document is a migration plan, not an instruction to delete the file immediately.

## Current Facts

As of August 17, 2026:

1. `TerminalAppLib.vcxproj` is a hand-maintained MSBuild project file, not a CMake-generated file.
2. The current `build\build.ninja`, `build\build-Release.ninja`, and `build\CMakeCache.txt` do not directly reference `TerminalAppLib.vcxproj`.
3. `microsoft/src/cascadia/TerminalApp/dll/TerminalApp.vcxproj` explicitly documents that source files and XAML files belong in `TerminalAppLib.vcxproj`.
4. `TerminalAppLib.vcxproj` currently owns the bulk of `TerminalApp` source/XAML inventory for the VS/MSBuild project graph.
5. `TerminalAppLib.vcxproj.metaproj` was stale generated garbage and has already been removed.

## What This Means

`TerminalAppLib.vcxproj` is not a direct dependency of the current CMake/Ninja build directory, but it is still a real dependency of the Visual Studio / MSBuild project structure under `microsoft/src/cascadia`.

Deleting it right now would not be a clean "remove unused file" change. It would be a deliberate removal of one branch of the Windows project system, and that requires migration first.

## Required Outcome Before Deletion

Before `TerminalAppLib.vcxproj` can be deleted, all of the following must be true:

1. No checked-in `.vcxproj` or `.sln` file references `TerminalAppLib.vcxproj`.
2. No checked-in project comments or instructions still require developers to place `TerminalApp` source/XAML there.
3. The remaining Windows project graph still builds or is intentionally retired as a whole.
4. The CMake/Ninja build remains green and continues to produce the same `WindowsTerminal`/portable outputs.

## Recommended Migration Path

### Phase 1: Inventory

1. Enumerate every checked-in reference to `TerminalAppLib.vcxproj`.
2. Enumerate every source, header, IDL, and XAML item owned by `TerminalAppLib.vcxproj`.
3. Separate those items into:
   - runtime C++ sources
   - XAML/UI assets
   - generated-code inputs
   - packaging/resource inputs

Deliverable:
`ext/docs/terminalapplib-vcxproj-inventory.md`

### Phase 2: Decide the End State

Choose one of these two end states:

1. Keep VS/MSBuild support.
   - Then the contents of `TerminalAppLib.vcxproj` must be moved into other checked-in `.vcxproj` files such as `TerminalApp.vcxproj`, or a replacement project must be created.
2. Retire VS/MSBuild support for this area.
   - Then all checked-in `.vcxproj` files that depend on this split need to be explicitly deprecated or removed together.

Without choosing one of these, deleting `TerminalAppLib.vcxproj` is just a broken intermediate state.

## Practical Execution Plan

### Option A: Preserve VS/MSBuild

1. Move `ClCompile`, `ClInclude`, `Page`, `ApplicationDefinition`, and related items out of `TerminalAppLib.vcxproj`.
2. Fold them into the surviving project files that actually build the same binary set.
3. Update `TerminalApp.vcxproj` comments that currently point developers at `TerminalAppLib.vcxproj`.
4. Fix all project references and filters.
5. Validate:
   - `cmake --build .\build --config Release --target full`
   - the expected Windows/MSBuild build entry point
6. Delete `TerminalAppLib.vcxproj` only after both build paths are green.

Risk:
XAML/codegen/resource embedding behavior may currently depend on the project split.

### Option B: Retire VS/MSBuild

1. Document that `cmake + ninja` is the only supported build path for this area.
2. Remove checked-in `.sln` / `.vcxproj` dependencies that rely on `TerminalAppLib.vcxproj`.
3. Remove or rewrite any tooling/scripts that invoke those projects.
4. Validate only the supported path:
   - `cmake --build .\build --config Release --target full`
   - portable packaging to `bin`
5. Delete `TerminalAppLib.vcxproj`.

Risk:
This is a broader repo policy change, not just a local cleanup.

## Blocking Technical Questions

These must be answered before implementation:

1. Is Visual Studio / MSBuild support still an explicit requirement for this repository?
2. Which checked-in solution or packaging projects still depend on the `TerminalAppLib` split?
3. Does XAML embedding or PRI/resource generation rely on `TerminalAppLib.vcxproj` being a static library project specifically?
4. Are any downstream scripts, CI jobs, or developer workflows still invoking `TerminalAppLib.vcxproj` or `TerminalApp.vcxproj`?

## Minimum Safe Next Step

The next safe step is not deletion.

The next safe step is:

1. produce a reference inventory,
2. choose whether VS/MSBuild support is preserved or retired,
3. then do the migration in one branch,
4. then delete `TerminalAppLib.vcxproj`.

## Do Not Do This

Do not:

1. delete `TerminalAppLib.vcxproj` alone,
2. leave `TerminalApp.vcxproj` comments pointing to a deleted file,
3. assume that "not referenced by current Ninja files" means "safe to remove from the repo".
