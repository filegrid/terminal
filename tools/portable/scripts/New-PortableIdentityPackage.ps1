[CmdletBinding()]
Param(
    [Parameter(Mandatory, HelpMessage = "Path to Terminal AppX/MSIX package")]
    [ValidateScript({ Test-Path $_ -PathType Leaf })]
    [string]
    $TerminalAppX,

    [Parameter(HelpMessage = "Output directory for sparse identity assets")]
    [string]
    $Destination = ".",

    [Parameter(HelpMessage = "Path to makeappx.exe")]
    [ValidateScript({ Test-Path $_ -PathType Leaf })]
    [string]
    $MakeAppxPath = "C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64\MakeAppx.exe",

    [Parameter(HelpMessage = "Sparse identity package name")]
    [string]
    $PackageName = "WindowsTerminalSparseDev",

    [Parameter(HelpMessage = "Sparse identity publisher subject")]
    [string]
    $Publisher = "CN=Windows Terminal Portable Dev",

    [Parameter(HelpMessage = "Sparse identity display name")]
    [string]
    $DisplayName = "Windows Terminal Portable Sparse",

    [Parameter(HelpMessage = "Sparse identity publisher display name")]
    [string]
    $PublisherDisplayName = "Windows Terminal Portable",

    [Parameter(HelpMessage = "Application Id to match the package manifest")]
    [string]
    $ApplicationId = "App",

    [Parameter(HelpMessage = "Optional PFX for signing the generated identity package")]
    [ValidateScript({ Test-Path $_ -PathType Leaf })]
    [string]
    $PfxPath = "",

    [Parameter(HelpMessage = "Optional password for the signing PFX")]
    [string]
    $CertificatePassword = ""
)

$ErrorActionPreference = "Stop"
$unsignedPackagePublisherOid = "OID.2.25.311729368913984317654407730594956997722=1"

if ([string]::IsNullOrWhiteSpace($PfxPath) -and $Publisher -notmatch [regex]::Escape($unsignedPackagePublisherOid)) {
    $Publisher = "$Publisher, $unsignedPackagePublisherOid"
}

if ($null -eq (Get-Item $MakeAppxPath -ErrorAction SilentlyContinue)) {
    throw "Could not find MakeAppx.exe at `"$MakeAppxPath`"."
}

$tempDir = Join-Path ([System.IO.Path]::GetTempPath()) "tmp$([Convert]::ToString((Get-Random 65535),16).PadLeft(4,'0')).tmp"
$null = New-Item -ItemType Directory -Path $tempDir

try {
    $appxManifestPath = Join-Path $tempDir "AppxManifest.xml"
    & tar.exe -x -f $TerminalAppX -C $tempDir AppxManifest.xml
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to extract AppxManifest.xml from `"$TerminalAppX`"."
    }

    [xml]$terminalManifest = Get-Content $appxManifestPath
    $version = $terminalManifest.Package.Identity.Version
    if ([string]::IsNullOrWhiteSpace($version)) {
        throw "Could not determine the package version from `"$TerminalAppX`"."
    }

    $resolvedDestination = [System.IO.Path]::GetFullPath($Destination)
    $identityRoot = Join-Path $resolvedDestination "identity-package"
    $assetsRoot = Join-Path $identityRoot "Assets"
    $packageManifestPath = Join-Path $identityRoot "AppxManifest.xml"
    $sidecarManifestPath = Join-Path $resolvedDestination "WindowsTerminal.exe.manifest"
    $identityPackagePath = Join-Path $resolvedDestination ("{0}_{1}.msix" -f $PackageName, $version)

    New-Item -ItemType Directory -Force -Path $assetsRoot | Out-Null

    @"
<?xml version="1.0" encoding="utf-8"?>
<Package IgnorableNamespaces="uap uap10 rescap"
  xmlns="http://schemas.microsoft.com/appx/manifest/foundation/windows10"
  xmlns:uap="http://schemas.microsoft.com/appx/manifest/uap/windows10"
  xmlns:uap10="http://schemas.microsoft.com/appx/manifest/uap/windows10/10"
  xmlns:rescap="http://schemas.microsoft.com/appx/manifest/foundation/windows10/restrictedcapabilities">
  <Identity Name="$PackageName" Publisher="$Publisher" Version="$version" ProcessorArchitecture="neutral" />
  <Properties>
    <DisplayName>$DisplayName</DisplayName>
    <PublisherDisplayName>$PublisherDisplayName</PublisherDisplayName>
    <Logo>Assets\StoreLogo.png</Logo>
    <uap10:AllowExternalContent>true</uap10:AllowExternalContent>
  </Properties>
  <Resources>
    <Resource Language="en-us" />
  </Resources>
  <Dependencies>
    <TargetDeviceFamily Name="Windows.Desktop" MinVersion="10.0.19041.0" MaxVersionTested="10.0.26100.0" />
  </Dependencies>
  <Capabilities>
    <rescap:Capability Name="runFullTrust" />
    <rescap:Capability Name="unvirtualizedResources" />
  </Capabilities>
  <Applications>
    <Application Id="$ApplicationId" Executable="WindowsTerminal.exe" uap10:TrustLevel="mediumIL" uap10:RuntimeBehavior="win32App">
      <uap:VisualElements
        AppListEntry="none"
        DisplayName="$DisplayName"
        Description="$DisplayName"
        BackgroundColor="transparent"
        Square150x150Logo="Assets\Square150x150Logo.png"
        Square44x44Logo="Assets\Square44x44Logo.png" />
    </Application>
  </Applications>
</Package>
"@ | Set-Content -Path $packageManifestPath -Encoding UTF8

    @"
<?xml version="1.0" encoding="utf-8" standalone="yes"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
  <msix xmlns="urn:schemas-microsoft-com:msix.v1"
        publisher="$Publisher"
        packageName="$PackageName"
        applicationId="$ApplicationId" />
</assembly>
"@ | Set-Content -Path $sidecarManifestPath -Encoding UTF8

    if (Test-Path $identityPackagePath -PathType Leaf) {
        Remove-Item $identityPackagePath -Force
    }

    & $MakeAppxPath pack /o /d $identityRoot /nv /p $identityPackagePath | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to create sparse identity package `"$identityPackagePath`"."
    }

    if (-not [string]::IsNullOrWhiteSpace($PfxPath)) {
        $signToolCandidates = Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\bin\*\x64\signtool.exe" -ErrorAction SilentlyContinue |
            Sort-Object FullName -Descending
        $signToolPath = $signToolCandidates | Select-Object -First 1 -ExpandProperty FullName
        if ([string]::IsNullOrWhiteSpace($signToolPath)) {
            throw "Could not find signtool.exe to sign the identity package."
        }

        $signArgs = @("sign", "/fd", "SHA256", "/f", $PfxPath)
        if (-not [string]::IsNullOrWhiteSpace($CertificatePassword)) {
            $signArgs += @("/p", $CertificatePassword)
        }
        $signArgs += $identityPackagePath

        & $signToolPath @signArgs | Out-Null
        if ($LASTEXITCODE -ne 0) {
            throw "Failed to sign sparse identity package `"$identityPackagePath`"."
        }
    }

    [PSCustomObject]@{
        IdentityPackagePath = $identityPackagePath
        SidecarManifestPath = $sidecarManifestPath
        Version = $version
        PackageName = $PackageName
        Publisher = $Publisher
        Signed = -not [string]::IsNullOrWhiteSpace($PfxPath)
    }
}
finally {
    Remove-Item -Recurse -Force $tempDir -ErrorAction SilentlyContinue
}
