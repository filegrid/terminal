@echo off
setlocal

set "_CONFIG=Debug"
if not "%_LAST_BUILD_CONF%"=="" set "_CONFIG=%_LAST_BUILD_CONF%"
set "_TARGET=full"

:parse_args
if "%~1"=="" goto build
if /I "%~1"=="dbg" set "_CONFIG=Debug"
if /I "%~1"=="rel" set "_CONFIG=Release"
if /I "%~1"=="no_clean" set "_TARGET=full"
if /I "%~1"=="audit" set "_CONFIG=AuditMode"
if /I "%~1"=="fuzz" set "_CONFIG=Fuzzing"
if /I "%~1"=="exclusive" (
    echo Project-level exclusive builds have no CMake/Ninja contract yet.
    exit /b 2
)
shift
goto parse_args

:build
for /f "usebackq delims=" %%R in (`git rev-parse --show-toplevel`) do set "_ROOT=%%R"
if "%_ROOT%"=="" (
    echo Could not locate the repository root with git.
    exit /b 2
)

cmake -S "%_ROOT%" -B "%_ROOT%\build"
if errorlevel 1 exit /b %errorlevel%

cmake --build "%_ROOT%\build" --config %_CONFIG% --target %_TARGET%
set "_build_result=%errorlevel%"
if not "%_build_result%"=="0" goto failed

for %%C in ("%_CONFIG%") do endlocal & set "_LAST_BUILD_CONF=%%~C" & exit /b 0

:failed
for %%E in ("%_build_result%") do endlocal & exit /b %%~E
