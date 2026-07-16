# ext DLL extraction plan

## Goal

Build the workspace/chat extension code as a dedicated DLL so `ext` implementation changes stop recompiling the main `TerminalAppLib` project.

## Target shape

1. `ext` workspace/chat implementation is built by a dedicated DLL project.
2. `TerminalAppLib` keeps only declarations, host state, and thin call sites.
3. `TerminalApp` links against the ext DLL import library and packages the ext DLL alongside the existing app binaries.
4. The portable build still produces the accepted `bin` portable artifacts.

## Build flows

1. `cmake --build .\build`
   - Rebuild only the ext workspace runtime through a standard CMake target, then repack portable from the latest existing MSIX while overlaying the fresh ext runtime files.
   - Use this when only `ext` implementation changed and the legacy `microsoft\src\...` projects did not need source edits.
2. `cmake --build .\build --target full`
   - Keep the explicit full portable rebuild path.
   - Use this when old-engineering projects changed or when the dependency graph itself was adjusted.

## Current correction

The ext-only path must stay on the public CMake entrypoint, but it must not shell out to PowerShell or hand-written `cl/link` scripts. The compile boundary needs to be expressed as normal CMake source, target, include, option, dependency, and output declarations so CMake owns the generated build graph.

## Implementation steps

### 1. Move ext compilation ownership

1. Create a dedicated DLL project under `ext\src\workspace\dll`.
2. Compile the current ext workspace/chat sources there:
   - `WorkspaceTerminalPageExtension.cpp`
   - chat helper/controller/store sources already pulled into `TerminalAppLib`
3. Remove those ext source files from `TerminalAppLib.vcxproj`.

### 2. Add an import/export boundary

1. Add a shared `WT_WORKSPACE_EXT_API` macro header under `ext\src\workspace`.
2. Mark the workspace-facing `TerminalPage` methods declared through the ext surface headers with that macro.
3. Mark the workspace factory entrypoints in `WorkspaceHostInterfaces.h` with that macro.

This keeps the current workspace method ownership on the ext side while letting `TerminalApp` import the implementations from the DLL.

### 3. Wire the main app to the DLL

1. Add a project reference from `TerminalApp.vcxproj` to the new ext DLL project so the final app link resolves the imported workspace symbols.
2. Keep `TerminalPage.cpp` / `TerminalPage.h` as the host-side registration point only.
3. Ensure the ext DLL is part of the package graph so `portable` carries it into the final layout.

### 4. Record the new boundary

1. Update `ext\READ` with the DLL extraction milestone.
2. Keep the remaining `TerminalPage` page-level UI references documented as intentional host wiring.

### 5. Replace the temporary PowerShell build path

1. Remove the `msix-ext -> pwsh -> Build-WorkspaceExtensionDirect.ps1` chain.
2. Switch the root `CMakeLists.txt` from `LANGUAGES NONE` to a real C++ project so the ext-only runtime can be modeled as a normal CMake target.
3. Declare a `WorkspaceExtension` shared-library target in CMake with:
   - the ext workspace/chat source list
   - the existing include roots
   - the existing compile definitions/options needed for the DLL boundary
   - the existing binary `.lib` dependencies that ext-only is allowed to reuse from the last full build
4. Keep `msix-ext` as a thin dependency target over the real `WorkspaceExtension` CMake target.
5. Leave `full` on the explicit full rebuild path.

### 6. Validate

1. Build with:
   - `cmake -S . -B .\build`
   - `cmake --build .\build`
   - `cmake --build .\build --target full`
2. Verify the accepted `bin` portable launchers still work.
3. Confirm the ext-only `ext` path no longer invokes PowerShell for compilation and no longer compiles ext implementation through `TerminalAppLib.vcxproj`.

## Expected result

After this change, ext workspace/chat implementation churn should compile through a dedicated standard-CMake ext target instead of `TerminalAppLib` or a PowerShell wrapper, while the existing portable packaging flow remains valid.
