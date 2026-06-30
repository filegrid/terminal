[CmdletBinding()]
Param(
    [Parameter(HelpMessage = "Build configuration")]
    [string]
    $Configuration = "Release",

    [Parameter(HelpMessage = "Build platform")]
    [ValidateSet("x64", "x86", "arm64")]
    [string]
    $Platform = "x64",

    [Parameter(HelpMessage = "Output directory for the generated portable artifact. Legacy Portable roots still use portable/msix subdirectories.")]
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
    $PlatformToolset = "",

    [Parameter(HelpMessage = "TargetFrameworkRootPath override")]
    [string]
    $TargetFrameworkRootPath = "",

    [Parameter(HelpMessage = "Build a portable distribution with a .portable marker")]
    [switch]
    $PortableMode = $true,

    [Parameter(HelpMessage = "Also emit sparse identity package assets for supported non-SID-500 validation")]
    [switch]
    $GenerateSparseIdentityPackage,

    [Parameter(HelpMessage = "Sparse identity package name")]
    [string]
    $SparseIdentityPackageName = "WindowsTerminalSparseDev",

    [Parameter(HelpMessage = "Sparse identity publisher subject")]
    [string]
    $SparseIdentityPublisher = "CN=Windows Terminal Portable Dev",

    [Parameter(HelpMessage = "Optional PFX to sign the sparse identity package")]
    [string]
    $SparseIdentityPfxPath = "",

    [Parameter(HelpMessage = "Optional password for the sparse identity PFX")]
    [string]
    $SparseIdentityCertificatePassword = ""
)

$ErrorActionPreference = "Stop"

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
$workspaceRoot = [System.IO.Path]::GetFullPath((Join-Path $repoRoot ".."))

if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $OutputPath = Join-Path $workspaceRoot "bin"
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

function Resolve-VSBuildContext {
    Param(
        [string] $ResolvedMSBuildPath
    )

    $msbuildDirectory = Split-Path $ResolvedMSBuildPath -Parent
    $vsInstallRoot = [System.IO.Path]::GetFullPath((Join-Path $msbuildDirectory "..\..\.."))
    $vcTargets180 = Join-Path $vsInstallRoot "MSBuild\Microsoft\VC\v180"
    $vcTargets170 = Join-Path $vsInstallRoot "MSBuild\Microsoft\VC\v170"

    if (Test-Path (Join-Path $vcTargets180 "Microsoft.Cpp.Default.props") -PathType Leaf) {
        return @{
            VsInstallRoot = $vsInstallRoot
            VisualStudioVersion = "18.0"
            VCTargetsPath = $vcTargets180
            PlatformToolset = "v145"
        }
    }

    if (Test-Path (Join-Path $vcTargets170 "Microsoft.Cpp.Default.props") -PathType Leaf) {
        return @{
            VsInstallRoot = $vsInstallRoot
            VisualStudioVersion = "17.0"
            VCTargetsPath = $vcTargets170
            PlatformToolset = "v143"
        }
    }

    throw "Could not resolve VC targets under `"$vsInstallRoot`" from MSBuild path `"$ResolvedMSBuildPath`"."
}

function Resolve-VcpkgTriplet {
    Param(
        [string] $ResolvedPlatform
    )

    switch ($ResolvedPlatform.ToLowerInvariant()) {
        "x64" { return "x64-windows-static" }
        "x86" { return "x86-windows-static" }
        "arm64" { return "arm64-windows-static" }
        default { throw "Unsupported vcpkg platform `"$ResolvedPlatform`"." }
    }
}

function Ensure-VcpkgDependencies {
    Param(
        [string] $ResolvedVcpkgRoot,
        [string] $ResolvedRepoRoot,
        [string] $ResolvedPlatform
    )

    $vcpkgExe = Join-Path $ResolvedVcpkgRoot "vcpkg.exe"
    if (-not (Test-Path $vcpkgExe -PathType Leaf)) {
        $bootstrapScript = Join-Path $ResolvedVcpkgRoot "bootstrap-vcpkg.bat"
        if (-not (Test-Path $bootstrapScript -PathType Leaf)) {
            throw "Could not find vcpkg.exe or bootstrap-vcpkg.bat under `"$ResolvedVcpkgRoot`"."
        }

        & $bootstrapScript | Out-Host
        if ($LASTEXITCODE -ne 0) {
            throw "vcpkg bootstrap failed with code $LASTEXITCODE"
        }
    }

    $installRoot = Join-Path $ResolvedRepoRoot "obj\$ResolvedPlatform\vcpkg"
    $triplet = Resolve-VcpkgTriplet -ResolvedPlatform $ResolvedPlatform
    $gslHeader = Join-Path $installRoot "$triplet\include\gsl\gsl_util"
    if (Test-Path $gslHeader -PathType Leaf) {
        return
    }

    $installArgs = @(
        "install",
        "--triplet", $triplet,
        "--x-manifest-root=$ResolvedRepoRoot",
        "--x-install-root=$installRoot",
        "--x-feature=terminal"
    )

    & $vcpkgExe @installArgs | Out-Host
    if ($LASTEXITCODE -ne 0) {
        throw "vcpkg install failed with code $LASTEXITCODE"
    }

    if (-not (Test-Path $gslHeader -PathType Leaf)) {
        throw "vcpkg install completed, but `"$gslHeader`" is still missing."
    }
}

function Publish-PortableArtifactAliases {
    Param(
        [Parameter(Mandatory)]
        [System.IO.FileInfo] $PortableArtifact,

        [Parameter(Mandatory)]
        [string] $ResolvedConfiguration,

        [Parameter(Mandatory)]
        [string] $ResolvedPlatform
    )

    $nameParts = $PortableArtifact.BaseName -split "_"
    $artifactVersion = if ($nameParts.Length -ge 3) { $nameParts[$nameParts.Length - 2] } else { "0.0.0.0" }
    $artifactPlatform = if ($nameParts.Length -ge 2) { $nameParts[$nameParts.Length - 1] } else { $ResolvedPlatform }

    $configurationSuffix = if ([System.StringComparer]::OrdinalIgnoreCase.Equals($ResolvedConfiguration, "Release")) {
        ""
    } else {
        "_$ResolvedConfiguration"
    }

    $legacyAliases = @(
        "WindowsTerminalPortable_en-US{0}_{1}_{2}{3}" -f $configurationSuffix, $artifactVersion, $artifactPlatform, $PortableArtifact.Extension
        "WindowsTerminalPortable_zh-CN{0}_{1}_{2}{3}" -f $configurationSuffix, $artifactVersion, $artifactPlatform, $PortableArtifact.Extension
    )
    $languageAliases = @(
        "WindowsTerminalPortableGeekEdition_System{0}_{1}_{2}{3}" -f $configurationSuffix, $artifactVersion, $artifactPlatform, $PortableArtifact.Extension
        "WindowsTerminalPortableGeekEdition_English{0}_{1}_{2}{3}" -f $configurationSuffix, $artifactVersion, $artifactPlatform, $PortableArtifact.Extension
    )

    foreach ($legacyAliasName in $legacyAliases) {
        $legacyAliasPath = Join-Path $PortableArtifact.DirectoryName $legacyAliasName
        if (Test-Path $legacyAliasPath -PathType Leaf) {
            Remove-Item $legacyAliasPath -Force
        }
    }

    foreach ($aliasName in $languageAliases) {
        $aliasPath = Join-Path $PortableArtifact.DirectoryName $aliasName
        if (Test-Path $aliasPath -PathType Leaf) {
            Remove-Item $aliasPath -Force
        }

        Copy-Item $PortableArtifact.FullName $aliasPath
    }
}

function Invoke-MSBuildProject {
    Param(
        [Parameter(Mandatory)]
        [string] $ProjectPath,

        [Parameter(Mandatory)]
        [string] $ResolvedConfiguration,

        [Parameter(Mandatory)]
        [string] $ResolvedPlatform,

        [Parameter(Mandatory)]
        [string] $ResolvedPlatformToolset,

        [Parameter(Mandatory)]
        [string] $ResolvedWindowsTargetPlatformVersion,

        [Parameter(Mandatory)]
        [string] $ResolvedTargetFrameworkRootPath,

        [Parameter(Mandatory)]
        [string] $ResolvedVisualStudioVersion
    )

    $projectBuildArgs = @(
        $ProjectPath,
        "/t:Build",
        "/m:1",
        "/p:Configuration=$ResolvedConfiguration",
        "/p:Platform=$ResolvedPlatform",
        "/p:PlatformToolset=$ResolvedPlatformToolset",
        "/p:WindowsTargetPlatformVersion=$ResolvedWindowsTargetPlatformVersion",
        "/p:VcpkgManifestInstall=false",
        "/p:VisualStudioVersion=$ResolvedVisualStudioVersion",
        "/p:OpenConsoleDir=$repoRoot\",
        "/p:SolutionDir=$repoRoot\",
        "/p:TargetFrameworkRootPath=$ResolvedTargetFrameworkRootPath"
    )

    & $msbuild @projectBuildArgs
    if ($LASTEXITCODE -ne 0) {
        throw "Building prerequisite project `"$ProjectPath`" failed with code $LASTEXITCODE"
    }
}

$msbuild = Resolve-MSBuildPath -ExplicitPath $MSBuildPath
$vsBuildContext = Resolve-VSBuildContext -ResolvedMSBuildPath $msbuild
if ([string]::IsNullOrWhiteSpace($PlatformToolset)) {
    $PlatformToolset = $vsBuildContext.PlatformToolset
}
if ([string]::IsNullOrWhiteSpace($VcpkgRoot)) {
    $vsVcpkgRoot = Join-Path $vsBuildContext.VsInstallRoot "VC\vcpkg"
    if (Test-Path (Join-Path $vsVcpkgRoot "vcpkg.exe") -PathType Leaf) {
        $VcpkgRoot = $vsVcpkgRoot
    } else {
        $VcpkgRoot = Join-Path $repoRoot "dep\vcpkg"
    }
}
$sdkRoot = Join-Path "C:\Program Files\dotnet\sdk" $DotnetSdkVersion
$sdkPath = Join-Path $sdkRoot "Sdks"
if (-not (Test-Path $sdkPath -PathType Container)) {
    throw "Could not find .NET SDK at `"$sdkPath`"."
}

$resolvedOutputPath = [System.IO.Path]::GetFullPath($OutputPath)
if ([System.StringComparer]::OrdinalIgnoreCase.Equals((Split-Path $resolvedOutputPath -Leaf), "Portable")) {
    $portableRoot = Join-Path $resolvedOutputPath "portable"
    $packageRoot = Join-Path $resolvedOutputPath "msix"
} else {
    $portableRoot = $resolvedOutputPath
    $packageRoot = Join-Path $resolvedOutputPath "msix"
}
New-Item -ItemType Directory -Force -Path $packageRoot, $portableRoot | Out-Null

$env:DOTNET_ROOT = "C:\Program Files\dotnet"
$env:PATH = "C:\Program Files\dotnet;$env:PATH"
$env:MSBuildSDKsPath = $sdkPath
$env:MSBuildEnableWorkloadResolver = "false"
$env:DOTNET_MSBUILD_SDK_RESOLVER_SDKS_DIR = $sdkPath
$env:DOTNET_MSBUILD_SDK_RESOLVER_SDKS_VER = $DotnetSdkVersion
$env:DOTNET_MSBUILD_SDK_RESOLVER_CLI_DIR = "C:\Program Files\dotnet"
$env:VCPKG_ROOT = $VcpkgRoot
$env:VisualStudioVersion = $vsBuildContext.VisualStudioVersion
$env:VCPKG_PLATFORM_TOOLSET = $PlatformToolset

Ensure-VcpkgDependencies -ResolvedVcpkgRoot $VcpkgRoot -ResolvedRepoRoot $repoRoot -ResolvedPlatform $Platform

$prerequisiteProjects = @(
    (Join-Path $repoRoot "src\host\proxy\Host.Proxy.vcxproj"),
    (Join-Path $repoRoot "src\cascadia\TerminalControl\dll\TerminalControl.vcxproj"),
    (Join-Path $repoRoot "src\cascadia\TerminalSettingsModel\dll\Microsoft.Terminal.Settings.Model.vcxproj")
)

foreach ($prerequisiteProject in $prerequisiteProjects) {
    Invoke-MSBuildProject `
        -ProjectPath $prerequisiteProject `
        -ResolvedConfiguration $Configuration `
        -ResolvedPlatform $Platform `
        -ResolvedPlatformToolset $PlatformToolset `
        -ResolvedWindowsTargetPlatformVersion $WindowsTargetPlatformVersion `
        -ResolvedTargetFrameworkRootPath $TargetFrameworkRootPath `
        -ResolvedVisualStudioVersion $vsBuildContext.VisualStudioVersion
}

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
    "/p:VisualStudioVersion=$($vsBuildContext.VisualStudioVersion)",
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

$portableOutput = & (Join-Path $PSScriptRoot "New-UnpackagedTerminalDistribution.ps1") @portableParams
if ($LASTEXITCODE -ne 0) {
    throw "Portable distribution generation failed with code $LASTEXITCODE"
}

if ($portableOutput -is [System.Array]) {
    $portableOutput = $portableOutput | Select-Object -Last 1
}

$portableOutputItem = Get-Item $portableOutput.FullName
Publish-PortableArtifactAliases -PortableArtifact $portableOutputItem -ResolvedConfiguration $Configuration -ResolvedPlatform $Platform

if ($GenerateSparseIdentityPackage) {
    $identityOutput = Join-Path $packageRoot "portable-identity"
    $identityParams = @{
        TerminalAppX = $terminalAppx
        Destination = $identityOutput
        PackageName = $SparseIdentityPackageName
        Publisher = $SparseIdentityPublisher
    }
    if (-not [string]::IsNullOrWhiteSpace($SparseIdentityPfxPath)) {
        $identityParams.PfxPath = $SparseIdentityPfxPath
    }
    if (-not [string]::IsNullOrWhiteSpace($SparseIdentityCertificatePassword)) {
        $identityParams.CertificatePassword = $SparseIdentityCertificatePassword
    }

    & (Join-Path $repoRoot "ext\src\portable\scripts\New-PortableIdentityPackage.ps1") @identityParams | Out-Host
    if ($LASTEXITCODE -ne 0) {
        throw "Sparse identity asset generation failed with code $LASTEXITCODE"
    }
}
