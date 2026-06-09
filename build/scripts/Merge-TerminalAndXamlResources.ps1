Param(
    [Parameter(Mandatory,
      HelpMessage="Root directory of extracted Terminal AppX")]
    [string[]]
    $TerminalRoot,

    [Parameter(Mandatory,
      HelpMessage="Root directory of extracted Xaml AppX")]
    [string[]]
    $XamlRoot,

    [Parameter(Mandatory,
      HelpMessage="Output Path")]
    [string]
    $OutputPath,

    [Parameter(HelpMessage="Path to makepri.exe")]
    [ValidateScript({Test-Path $_ -Type Leaf})]
    [string]
    $MakePriPath = "C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64\MakePri.exe"
)

$ErrorActionPreference = 'Stop'

$tempDir = Join-Path ([System.IO.Path]::GetTempPath()) "tmp$([Convert]::ToString((Get-Random 65535),16).PadLeft(4,'0')).tmp"
New-Item -ItemType Directory -Path $tempDir | Out-Null

function Read-XmlDocumentWithRetry {
    Param(
        [Parameter(Mandatory)]
        [string] $Path,

        [int] $Attempts = 10,

        [int] $DelayMilliseconds = 200
    )

    for ($attempt = 1; $attempt -le $Attempts; $attempt++) {
        try {
            $content = [System.IO.File]::ReadAllText($Path, [System.Text.Encoding]::UTF8)
            if ([string]::IsNullOrWhiteSpace($content)) {
                throw "The XML file `"$Path`" is empty."
            }

            return [xml]$content
        } catch {
            if ($attempt -eq $Attempts) {
                throw
            }

            Start-Sleep -Milliseconds $DelayMilliseconds
        }
    }
}

$terminalDump = Join-Path $tempDir "terminal.pri.xml"

& $MakePriPath dump /if (Join-Path $TerminalRoot "resources.pri") /of $terminalDump /dt detailed
if ($LASTEXITCODE -ne 0) {
    throw "Dumping Terminal resources PRI failed with code $LASTEXITCODE"
}

Write-Verbose "Removing Microsoft.UI.Xaml node from Terminal to prevent a collision with XAML"
$terminalXMLDocument = Read-XmlDocumentWithRetry -Path $terminalDump
$resourceMap = $terminalXMLDocument.PriInfo.ResourceMap
$fileSubtree = $resourceMap.ResourceMapSubtree | Where-Object { $_.Name -eq "Files" }
$subtrees = $fileSubtree.ResourceMapSubtree
$xamlSubtreeChild = ($subtrees | Where-Object { $_.Name -eq "Microsoft.UI.Xaml" })
if ($Null -Ne $xamlSubtreeChild) {
    $null = $fileSubtree.RemoveChild($xamlSubtreeChild)
    $terminalXMLDocument.Save($terminalDump)
}

$indexName = $terminalXMLDocument.PriInfo.ResourceMap.name

& (Join-Path $PSScriptRoot "Merge-PriFiles.ps1") -Path $terminalDump, (Join-Path $XamlRoot "resources.pri") -IndexName $indexName -OutputPath $OutputPath -MakePriPath $MakePriPath

Remove-Item -Recurse -Force $tempDir
