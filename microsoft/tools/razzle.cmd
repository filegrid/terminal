@echo off

if not "%OpenConBuild%"=="" goto ready

pushd "%~dp0" >nul
for /f "delims=" %%R in ('git rev-parse --show-toplevel') do set "OPENCON_ROOT=%%R"
popd
if "%OPENCON_ROOT%"=="" (
    echo Could not locate the repository root with git.
    exit /b 2
)

set "OPENCON=%OPENCON_ROOT%\microsoft"
set "OPENCON_TOOLS=%OPENCON%\tools\"
set "PATH=%OPENCON_TOOLS%;%OPENCON%\dep\nuget;%PATH%"
set "ARCH=x64"
set "PLATFORM=x64"
set "DEFAULT_CONFIGURATION=Debug"

:parse_args
if "%~1"=="" goto ready
if /I "%~1"=="dbg" set "DEFAULT_CONFIGURATION=Debug"
if /I "%~1"=="rel" set "DEFAULT_CONFIGURATION=Release"
if /I "%~1"=="x86" (
    set "ARCH=x86"
    set "PLATFORM=x86"
)
shift
goto parse_args

:ready
set "OpenConBuild=true"
echo The CMake/Ninja development environment is ready.
