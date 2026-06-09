# Portable build agent notes

## Build target

Build the portable single-file Windows Terminal package with:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\build\scripts\Build-PortableTerminalDistribution.ps1
```

The output artifact is:

`AppPackages\Portable\portable\WindowsTerminalDev_0.0.1.0_x64.exe`

## Acceptance after build

After every portable build, run the produced exe instead of only checking that the file exists.

Minimum acceptance:

1. Launch `AppPackages\Portable\portable\WindowsTerminalDev_0.0.1.0_x64.exe`
2. Confirm the launcher exits with code `0`
3. Confirm the payload is extracted under `%LOCALAPPDATA%\WTP\<hash>\p`
4. Confirm `%LOCALAPPDATA%\WTP\<hash>\p\terminal.marker` exists
5. Confirm no new `%LOCALAPPDATA%\Microsoft\Windows Terminal\settings.json` is created by the launcher
6. Confirm the launcher points settings to `%LOCALAPPDATA%\Packages\Microsoft.WindowsTerminal_8wekyb3d8bbwe\LocalState`

If the environment can keep the GUI process alive, also confirm `WindowsTerminal.exe` starts from the extracted payload directory.

## Packaging and unpacking directory strategy

### Build-time packaging

`Build-PortableTerminalDistribution.ps1` writes:

- `AppPackages\Portable\msix\...` for the intermediate MSIX package
- `AppPackages\Portable\portable\WindowsTerminalDev_0.0.1.0_x64.exe` for the final single-file portable launcher

The single-file exe is produced by compiling `src\tools\PortableTerminalLauncher\Program.cs`, then appending the packaged payload zip and the `WTPORT01` footer metadata.

### Runtime unpacking and settings

The launcher extracts into a short cache root to avoid Windows MAX_PATH failures:

`%LOCALAPPDATA%\WTP\<12-char hash>`

Layout inside that cache root:

- `p` — final extracted payload directory
- `c` — extraction completion sentinel
- `x` — temporary extraction directory used only during refresh
- `x\p.zip` — temporary embedded payload copy before unzip

The hash is derived from the launcher path, file length, and last write time. Re-running the same exe reuses the existing `p` directory when both `c` and `p\terminal.marker` are present.

The single-file launcher does not use the unpackaged `%LOCALAPPDATA%\Microsoft\Windows Terminal` settings root. It sets `WT_SETTINGS_DIR_OVERRIDE` so the unpackaged process reads and writes settings from the installed stable Terminal path:

`%LOCALAPPDATA%\Packages\Microsoft.WindowsTerminal_8wekyb3d8bbwe\LocalState`
