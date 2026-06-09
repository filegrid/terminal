[CmdletBinding()]
Param(
    [Parameter(HelpMessage = "Build configuration")]
    [string]
    $Configuration = "Debug",

    [Parameter(HelpMessage = "Build platform")]
    [ValidateSet("x64", "x86", "arm64")]
    [string]
    $Platform = "x64",

    [Parameter(HelpMessage = "Output directory for the generated package and portable artifact")]
    [string]
    $OutputPath = "",

    [Parameter(HelpMessage = "Path to MSBuild.exe")]
    [string]
    $MSBuildPath = "",

    [Parameter(HelpMessage = "Path to vcpkg root")]
    [string]
    $VcpkgRoot = "",

    [Parameter(HelpMessage = "Pinned .NET SDK version")]
    [string]
    $DotnetSdkVersion = "8.0.421",

    [Parameter(HelpMessage = "Windows SDK target version")]
    [string]
    $WindowsTargetPlatformVersion = "10.0.22621.0",

    [Parameter(HelpMessage = "Platform toolset")]
    [string]
    $PlatformToolset = "v145",

    [Parameter(HelpMessage = "TargetFrameworkRootPath override")]
    [string]
    $TargetFrameworkRootPath = "",

    [Parameter(HelpMessage = "Build a portable distribution with a .portable marker")]
    [switch]
    $PortableMode = $true
)

$ErrorActionPreference = "Stop"

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))

if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $OutputPath = Join-Path $repoRoot "AppPackages\Portable"
}
if ([string]::IsNullOrWhiteSpace($VcpkgRoot)) {
    $VcpkgRoot = Join-Path $repoRoot "dep\vcpkg"
}
if ([string]::IsNullOrWhiteSpace($TargetFrameworkRootPath)) {
    $TargetFrameworkRootPath = Join-Path $repoRoot "packages-ref\Microsoft.NETFramework.ReferenceAssemblies.net472.1.0.3\build\"
}

function Resolve-MSBuildPath {
    Param(
        [string] $ExplicitPath
    )

    if (-not [string]::IsNullOrWhiteSpace($ExplicitPath)) {
        return $ExplicitPath
    }

    $vs2026 = "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe"
    if (Test-Path $vs2026 -PathType Leaf) {
        return $vs2026
    }

    $command = Get-Command msbuild.exe -ErrorAction SilentlyContinue
    if ($null -ne $command) {
        return $command.Source
    }

    throw "Could not find MSBuild.exe. Pass -MSBuildPath explicitly."
}

$msbuild = Resolve-MSBuildPath -ExplicitPath $MSBuildPath
$sdkRoot = Join-Path "C:\Program Files\dotnet\sdk" $DotnetSdkVersion
$sdkPath = Join-Path $sdkRoot "Sdks"
if (-not (Test-Path $sdkPath -PathType Container)) {
    throw "Could not find .NET SDK at `"$sdkPath`"."
}

$packageRoot = Join-Path $OutputPath "msix"
$portableRoot = Join-Path $OutputPath "portable"
New-Item -ItemType Directory -Force -Path $packageRoot, $portableRoot | Out-Null

$env:DOTNET_ROOT = "C:\Program Files\dotnet"
$env:PATH = "C:\Program Files\dotnet;$env:PATH"
$env:MSBuildSDKsPath = $sdkPath
$env:MSBuildEnableWorkloadResolver = "false"
$env:DOTNET_MSBUILD_SDK_RESOLVER_SDKS_DIR = $sdkPath
$env:DOTNET_MSBUILD_SDK_RESOLVER_SDKS_VER = $DotnetSdkVersion
$env:DOTNET_MSBUILD_SDK_RESOLVER_CLI_DIR = "C:\Program Files\dotnet"
$env:VCPKG_ROOT = $VcpkgRoot

$wapproj = Join-Path $repoRoot "src\cascadia\CascadiaPackage\CascadiaPackage.wapproj"
$buildArgs = @(
    $wapproj,
    "/t:Build",
    "/m:1",
    "/p:Configuration=$Configuration",
    "/p:Platform=$Platform",
    "/p:PlatformToolset=$PlatformToolset",
    "/p:WindowsTargetPlatformVersion=$WindowsTargetPlatformVersion",
    "/p:VcpkgManifestInstall=false",
    "/p:GenerateAppxPackageOnBuild=true",
    "/p:OpenConsoleDir=$repoRoot\",
    "/p:SolutionDir=$repoRoot\",
    "/p:TargetFrameworkRootPath=$TargetFrameworkRootPath",
    "/p:AppxPackageDir=$packageRoot\"
)

& $msbuild @buildArgs
if ($LASTEXITCODE -ne 0) {
    throw "Packaging failed with code $LASTEXITCODE"
}

$terminalAppx = Get-ChildItem $packageRoot -Recurse -File -Include *.msix, *.appx |
    Where-Object { $_.Name -like "CascadiaPackage_*" } |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1 -ExpandProperty FullName

if (-not $terminalAppx) {
    throw "Could not find a generated Terminal package under `"$packageRoot`"."
}

$xamlAppx = Get-ChildItem $packageRoot -Recurse -File -Filter "Microsoft.UI.Xaml*.appx" |
    Where-Object { $_.FullName -match [regex]::Escape("\Dependencies\$Platform\") } |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1 -ExpandProperty FullName

if (-not $xamlAppx) {
    throw "Could not find the Microsoft.UI.Xaml dependency package for $Platform."
}

$portableParams = @{
    TerminalAppX = $terminalAppx
    XamlAppX = $xamlAppx
    Destination = $portableRoot
}
if ($PortableMode) {
    $portableParams.PortableMode = $true
$portableParams.SingleFileOutput = $true
}

& (Join-Path $PSScriptRoot "New-UnpackagedTerminalDistribution.ps1") @portableParams
if ($LASTEXITCODE -ne 0) {
    throw "Portable distribution generation failed with code $LASTEXITCODE"
}
