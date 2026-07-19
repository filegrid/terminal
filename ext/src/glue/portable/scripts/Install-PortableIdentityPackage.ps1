[CmdletBinding(SupportsShouldProcess)]
Param(
    [Parameter(Mandatory, HelpMessage = "Path to the generated sparse identity package for optional package-identity diagnostics")]
    [ValidateScript({ Test-Path $_ -PathType Leaf })]
    [string]
    $IdentityPackagePath,

    [Parameter(Mandatory, HelpMessage = "External content directory that contains WindowsTerminal.exe for optional package-identity diagnostics")]
    [ValidateScript({ Test-Path $_ -PathType Container })]
    [string]
    $ExternalLocation,

    [Parameter(HelpMessage = "Path to the sidecar WindowsTerminal.exe.manifest")]
    [ValidateScript({ Test-Path $_ -PathType Leaf })]
    [string]
    $SidecarManifestPath = "",

    [Parameter(HelpMessage = "Allow installing an unsigned sparse package for local validation")]
    [switch]
    $AllowUnsigned,

    [Parameter(HelpMessage = "Force the sparse identity package to update an existing registration")]
    [switch]
    $ForceUpdateFromAnyVersion = $true
)

$ErrorActionPreference = "Stop"
$unsignedPackagePublisherOid = "OID.2.25.311729368913984317654407730594956997722=1"

function Test-IsBuiltInAdministratorAccount {
    $identity = [System.Security.Principal.WindowsIdentity]::GetCurrent()
    return $null -ne $identity.User -and $identity.User.Value.EndsWith("-500", [System.StringComparison]::Ordinal)
}

function Get-IdentityPackagePublisher {
    Param(
        [Parameter(Mandatory)]
        [string] $PackagePath
    )

    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $zip = [System.IO.Compression.ZipFile]::OpenRead($PackagePath)
    try {
        $manifestEntry = $zip.Entries | Where-Object { $_.FullName -eq "AppxManifest.xml" } | Select-Object -First 1
        if ($null -eq $manifestEntry) {
            throw "Could not find AppxManifest.xml in `"$PackagePath`"."
        }

        $reader = [System.IO.StreamReader]::new($manifestEntry.Open())
        try {
            [xml]$manifest = $reader.ReadToEnd()
        }
        finally {
            $reader.Dispose()
        }

        return [string]$manifest.Package.Identity.Publisher
    }
    finally {
        $zip.Dispose()
    }
}

if (Test-IsBuiltInAdministratorAccount) {
    throw "Sparse identity validation is not supported from the built-in Administrator (SID-500) account on this machine. Use a regular desktop user or admin account instead."
}

$resolvedIdentityPackagePath = (Resolve-Path $IdentityPackagePath).Path
$resolvedExternalLocation = (Resolve-Path $ExternalLocation).Path
$externalExecutablePath = Join-Path $resolvedExternalLocation "WindowsTerminal.exe"
if (-not (Test-Path $externalExecutablePath -PathType Leaf)) {
    throw "Could not find WindowsTerminal.exe under `"$resolvedExternalLocation`"."
}

if ([string]::IsNullOrWhiteSpace($SidecarManifestPath)) {
    $defaultSidecarManifestPath = Join-Path (Split-Path $resolvedIdentityPackagePath -Parent) "WindowsTerminal.exe.manifest"
    if (Test-Path $defaultSidecarManifestPath -PathType Leaf) {
        $SidecarManifestPath = $defaultSidecarManifestPath
    }
}

if ([string]::IsNullOrWhiteSpace($SidecarManifestPath)) {
    throw "Could not find WindowsTerminal.exe.manifest automatically. Pass -SidecarManifestPath explicitly."
}

$resolvedSidecarManifestPath = (Resolve-Path $SidecarManifestPath).Path
$targetSidecarManifestPath = Join-Path $resolvedExternalLocation "WindowsTerminal.exe.manifest"

if (-not [System.StringComparer]::OrdinalIgnoreCase.Equals($resolvedSidecarManifestPath, $targetSidecarManifestPath)) {
    Copy-Item -Path $resolvedSidecarManifestPath -Destination $targetSidecarManifestPath -Force
}

$addAppxParams = @{
    Path = $resolvedIdentityPackagePath
    ExternalLocation = $resolvedExternalLocation
}
if ($ForceUpdateFromAnyVersion) {
    $addAppxParams.ForceUpdateFromAnyVersion = $true
}
if ($AllowUnsigned) {
    $publisher = Get-IdentityPackagePublisher -PackagePath $resolvedIdentityPackagePath
    if ([string]::IsNullOrWhiteSpace($publisher) -or $publisher -notmatch [regex]::Escape($unsignedPackagePublisherOid)) {
        throw "The sparse identity package publisher must include $unsignedPackagePublisherOid when using -AllowUnsigned. Rebuild the package with New-PortableIdentityPackage.ps1 or Build-PortableTerminalDistribution.ps1."
    }
    $addAppxParams.AllowUnsigned = $true
}

if ($PSCmdlet.ShouldProcess($resolvedIdentityPackagePath, "Install sparse identity package for $resolvedExternalLocation")) {
    Add-AppxPackage @addAppxParams
}

[PSCustomObject]@{
    IdentityPackagePath = $resolvedIdentityPackagePath
    ExternalLocation = $resolvedExternalLocation
    SidecarManifestPath = $targetSidecarManifestPath
    AllowUnsigned = $AllowUnsigned.IsPresent
    ForceUpdateFromAnyVersion = $ForceUpdateFromAnyVersion.IsPresent
}
