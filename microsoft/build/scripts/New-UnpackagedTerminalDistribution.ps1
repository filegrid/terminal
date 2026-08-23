[CmdletBinding(DefaultParameterSetName = 'AppX')]
Param(
    [Parameter(Mandatory, HelpMessage="Path to Terminal AppX", ParameterSetName = 'AppX')]
    [ValidateScript({Test-Path $_ -Type Leaf})]
    [string]
    $TerminalAppX,

    [Parameter(Mandatory, HelpMessage="Path to Terminal Layout Deployment", ParameterSetName='Layout')]
    [ValidateScript({Test-Path $_ -Type Container})]
    [string]
    $TerminalLayout,

    [Parameter(Mandatory, HelpMessage="Path to Xaml AppX", ParameterSetName='AppX')]
    [Parameter(Mandatory, HelpMessage="Path to Xaml AppX", ParameterSetName='Layout')]
    [ValidateScript({Test-Path $_ -Type Leaf})]
    [string]
    $XamlAppX,

    [Parameter(HelpMessage="Output Directory", ParameterSetName='AppX')]
    [Parameter(HelpMessage="Output Directory", ParameterSetName='Layout')]
    [string]
    $Destination = ".",

    [Parameter(HelpMessage="Path to makeappx.exe", ParameterSetName='AppX')]
    [Parameter(HelpMessage="Path to makeappx.exe", ParameterSetName='Layout')]
    [ValidateScript({Test-Path $_ -Type Leaf})]
    [string]
    $MakeAppxPath = "C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64\MakeAppx.exe",

    [Parameter(HelpMessage="Include the portable mode marker file by default", ParameterSetName='AppX')]
    [Parameter(HelpMessage="Include the portable mode marker file by default", ParameterSetName='Layout')]
    [switch]
    $PortableMode = $PSCmdlet.ParameterSetName -eq 'Layout',

    [Parameter(HelpMessage="Bundle the portable distribution into a single executable", ParameterSetName='AppX')]
    [switch]
    $SingleFileOutput
)

$filesToRemove = @("*.xml", "*.winmd", "Appx*", "Images/*Tile*", "Images/*Logo*") # Remove from Terminal
$filesToKeep = @() # ... except for these
$filesToCopyFromXaml = @("Microsoft.UI.Xaml.dll", "Microsoft.UI.Xaml") # We don't need the .winmd

$ErrorActionPreference = 'Stop'

If ($null -Eq (Get-Item $MakeAppxPath -EA:SilentlyContinue)) {
    Write-Error "Could not find MakeAppx.exe at `"$MakeAppxPath`".`nMake sure that -MakeAppxPath points to a valid SDK."
    Exit 1
}

$tempDir = Join-Path ([System.IO.Path]::GetTempPath()) "tmp$([Convert]::ToString((Get-Random 65535),16).PadLeft(4,'0')).tmp"
New-Item -ItemType Directory -Path $tempDir | Out-Null

$XamlAppX = Get-Item $XamlAppX | Select-Object -Expand FullName

########
# Reading the AppX Manifest for preliminary info
########

If ($TerminalAppX) {
	$appxManifestPath = Join-Path $tempDir AppxManifest.xml
	& tar.exe -x -f "$TerminalAppX" -C $tempDir AppxManifest.xml
} ElseIf($TerminalLayout) {
	$appxManifestPath = Join-Path $TerminalLayout AppxManifest.xml
}
$manifest = [xml](Get-Content $appxManifestPath)
$pfn = $manifest.Package.Identity.Name
$version = $manifest.Package.Identity.Version
$architecture = $manifest.Package.Identity.ProcessorArchitecture

$distributionName = "{0}_{1}_{2}" -f ($pfn, $version, $architecture)
$terminalDir = "terminal-{0}" -f ($version)

if ($PortableMode -and -not $PSBoundParameters.ContainsKey('SingleFileOutput')) {
    $SingleFileOutput = $true
}

if ($SingleFileOutput -and -not $PortableMode) {
    throw "SingleFileOutput requires PortableMode."
}

########
# Unpacking Terminal and XAML
########

$terminalAppPath = Join-Path $tempdir $terminalDir

If ($TerminalAppX) {
	$TerminalAppX = Get-Item $TerminalAppX | Select-Object -Expand FullName
	New-Item -ItemType Directory -Path $terminalAppPath | Out-Null
	& $MakeAppxPath unpack /p $TerminalAppX /d $terminalAppPath /o | Out-Null
	If ($LASTEXITCODE -Ne 0) {
	    Throw "Unpacking $TerminalAppX failed"
	}
} ElseIf ($TerminalLayout) {
	Copy-Item -Recurse -Path $TerminalLayout -Destination $terminalAppPath
}

$xamlAppPath = Join-Path $tempdir "xaml"
New-Item -ItemType Directory -Path $xamlAppPath | Out-Null
& $MakeAppxPath unpack /p $XamlAppX /d $xamlAppPath /o | Out-Null
If ($LASTEXITCODE -Ne 0) {
    Throw "Unpacking $XamlAppX failed"
}

########
# Some sanity checking
########

$xamlManifest = [xml](Get-Content (Join-Path $xamlAppPath "AppxManifest.xml"))
If ($xamlManifest.Package.Identity.Name -NotLike "Microsoft.UI.Xaml*") {
    Throw "$XamlAppX is not a XAML package (instead, it looks like $($xamlManifest.Package.Identity.Name))"
}
If ($xamlManifest.Package.Identity.ProcessorArchitecture -Ne $architecture) {
    Throw "$XamlAppX is not built for $architecture (instead, it is built for $($xamlManifest.Package.Identity.ProcessorArchitecture))"
}

########
# Preparation of source files
########

$itemsToRemove = $filesToRemove | ForEach-Object {
    Get-Item (Join-Path $terminalAppPath $_) -EA:SilentlyContinue | Where-Object {
        $filesToKeep -NotContains $_.Name
    }
} | Sort-Object FullName -Unique
$itemsToRemove | Remove-Item -Recurse

$filesToCopyFromXaml | ForEach-Object {
    Get-Item (Join-Path $xamlAppPath $_)
} | Copy-Item -Recurse -Destination $terminalAppPath

########
# Resource Management
########

$finalTerminalPriFile = Join-Path $terminalAppPath "resources.pri"
& (Join-Path $PSScriptRoot "Merge-TerminalAndXamlResources.ps1") `
    -TerminalRoot $terminalAppPath `
    -XamlRoot $xamlAppPath `
    -OutputPath $finalTerminalPriFile `
    -Verbose:$Verbose | Out-Host

########
# Packaging
########

$portableModeMarkerFile = Join-Path $terminalAppPath ".portable"
If ($PortableMode) {
    if (-not $SingleFileOutput) {
	    "" | Out-File $portableModeMarkerFile

        $settingsDirectory = Join-Path $terminalAppPath "settings"
        New-Item -ItemType Directory -Path $settingsDirectory -Force | Out-Null

        @'
{
  "profiles":
  {
    "defaults":
    {
      "cursorShape": "vintage"
    }
  }
}
'@ | Out-File -FilePath (Join-Path $settingsDirectory "settings.json") -Encoding utf8
    }
}

If ($PSCmdlet.ParameterSetName -Eq "AppX") {
	New-Item -ItemType Directory -Path $Destination -ErrorAction:SilentlyContinue | Out-Null
    $outputZip = Join-Path $tempDir ("{0}.zip" -f ($distributionName))
	& tar -c --format=zip -f $outputZip -C $tempDir $terminalDir

    if ($SingleFileOutput) {
        $launcherSourcePath = Join-Path $PSScriptRoot "..\..\src\tools\PortableTerminalLauncher\Program.cs"
        $launcherIconPath = Join-Path $PSScriptRoot "..\..\res\terminal.ico"
        $dotnetPath = "C:\Program Files\dotnet\dotnet.exe"
        $roslynCsc = "C:\Program Files\dotnet\sdk\8.0.421\Roslyn\bincore\csc.dll"
        $net472ReferenceRoot = Join-Path $PSScriptRoot "..\..\packages-ref\Microsoft.NETFramework.ReferenceAssemblies.net472.1.0.3\build\.NETFramework\v4.7.2"
        if (-not (Test-Path $dotnetPath -PathType Leaf) -or -not (Test-Path $roslynCsc -PathType Leaf)) {
            throw "Could not find the pinned .NET SDK 8.0.421 Roslyn compiler."
        }
        if (-not (Test-Path $net472ReferenceRoot -PathType Container)) {
            throw "Could not find .NET Framework 4.7.2 reference assemblies at `"$net472ReferenceRoot`"."
        }
        if (-not (Test-Path $launcherIconPath -PathType Leaf)) {
            throw "Could not find the portable launcher icon at `"$launcherIconPath`"."
        }

        $cscPlatform = switch ($architecture) {
            "x64" { "x64" }
            "x86" { "x86" }
            "arm64" { "arm64" }
            default { throw "Unsupported architecture $architecture for single-file portable output." }
        }

        $publishDir = Join-Path $tempDir "portable-launcher"
        New-Item -ItemType Directory -Path $publishDir -Force | Out-Null

        $assemblyInfoPath = Join-Path $publishDir "PortableTerminalLauncher.AssemblyInfo.cs"
        @"
using System.Reflection;
[assembly: AssemblyTitle("Windows Terminal Portable")]
[assembly: AssemblyProduct("Windows Terminal Portable")]
[assembly: AssemblyVersion("$version")]
[assembly: AssemblyFileVersion("$version")]
[assembly: AssemblyInformationalVersion("$version")]
"@ | Set-Content -Path $assemblyInfoPath -Encoding Ascii

        $outputExe = Join-Path $Destination ("{0}.exe" -f ($distributionName))
        $staleZip = Join-Path $Destination ("{0}.zip" -f ($distributionName))
        if (Test-Path $staleZip -PathType Leaf) {
            Remove-Item $staleZip -Force
        }

        $launcherPath = Join-Path $publishDir "PortableTerminalLauncher.exe"
        $compilerResponsePath = Join-Path $publishDir "PortableTerminalLauncher.rsp"
        $compilerArguments = @(
            "/target:winexe",
            "/optimize+",
            "/platform:$cscPlatform",
            "/win32icon:$launcherIconPath",
            "/out:$launcherPath"
        )
        Get-ChildItem $net472ReferenceRoot -Filter "*.dll" -File |
            Where-Object { $_.Name -notin "System.EnterpriseServices.Wrapper.dll", "System.EnterpriseServices.Thunk.dll" } |
            ForEach-Object { $compilerArguments += "/reference:$($_.FullName)" }
        $compilerArguments += $assemblyInfoPath, $launcherSourcePath
        Set-Content -Path $compilerResponsePath -Value $compilerArguments -Encoding Ascii

        & $dotnetPath $roslynCsc /noconfig "@$compilerResponsePath"
        if ($LASTEXITCODE -ne 0) {
            throw "Building the single-file portable launcher failed with code $LASTEXITCODE"
        }

        if (-not (Test-Path $launcherPath -PathType Leaf)) {
            throw "Could not find the published portable launcher at `"$launcherPath`"."
        }

        if (Test-Path $outputExe -PathType Leaf) {
            Remove-Item $outputExe -Force
        }

        Copy-Item $launcherPath $outputExe

        $payloadLength = (Get-Item $outputZip).Length
        $payloadLengthBytes = [BitConverter]::GetBytes([int64]$payloadLength)
        $footerMagic = [System.Text.Encoding]::ASCII.GetBytes("WTPORT01")

        $outputStream = [System.IO.File]::Open($outputExe, [System.IO.FileMode]::Append, [System.IO.FileAccess]::Write, [System.IO.FileShare]::Read)
        try {
            $payloadStream = [System.IO.File]::OpenRead($outputZip)
            try {
                $payloadStream.CopyTo($outputStream)
                $outputStream.Write($payloadLengthBytes, 0, $payloadLengthBytes.Length)
                $outputStream.Write($footerMagic, 0, $footerMagic.Length)
            }
            finally {
                $payloadStream.Dispose()
            }
        }
        finally {
            $outputStream.Dispose()
        }

	    Remove-Item -Recurse -Force $tempDir -EA:SilentlyContinue
        Get-Item $outputExe
    } else {
	    $finalZip = Join-Path $Destination ("{0}.zip" -f ($distributionName))
        $staleExe = Join-Path $Destination ("{0}.exe" -f ($distributionName))
        if (Test-Path $staleExe -PathType Leaf) {
            Remove-Item $staleExe -Force
        }
        Move-Item $outputZip $finalZip -Force
	    Remove-Item -Recurse -Force $tempDir -EA:SilentlyContinue
	    Get-Item $finalZip
    }
} ElseIf ($PSCmdlet.ParameterSetName -Eq "Layout") {
	Get-Item $terminalAppPath
}
