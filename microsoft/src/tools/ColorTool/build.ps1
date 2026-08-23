[CmdletBinding()]
param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$Arguments
)

$ErrorActionPreference = 'Stop'
$projectDirectory = Join-Path $PSScriptRoot 'ColorTool'
$clean = $Arguments -contains 'clean'
$configuration = if ($Arguments -contains 'rel') { 'Release' } else { 'Debug' }
$outputDirectory = Join-Path $projectDirectory "bin\\$configuration"

if ($clean -and (Test-Path -LiteralPath $outputDirectory)) {
    Remove-Item -LiteralPath $outputDirectory -Recurse -Force
}

New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null

$dotnet = (Get-Command dotnet -ErrorAction Stop).Source
$csc = Get-ChildItem (Join-Path $env:ProgramFiles 'dotnet\\sdk') -Directory |
    Sort-Object Name -Descending |
    ForEach-Object { Join-Path $_.FullName 'Roslyn\\bincore\\csc.dll' } |
    Where-Object { Test-Path -LiteralPath $_ } |
    Select-Object -First 1
if (-not $csc) { throw 'The .NET SDK C# compiler was not found.' }

$resgen = Get-ChildItem (Join-Path ${env:ProgramFiles(x86)} 'Microsoft SDKs\\Windows') -Recurse -Filter ResGen.exe |
    Select-Object -First 1 -ExpandProperty FullName
if (-not $resgen) { throw 'Windows SDK ResGen.exe was not found.' }

$frameworkDirectory = Join-Path $env:WINDIR 'Microsoft.NET\\Framework64\\v4.0.30319'
if (-not (Test-Path -LiteralPath $frameworkDirectory)) {
    $frameworkDirectory = Join-Path $env:WINDIR 'Microsoft.NET\\Framework\\v4.0.30319'
}

$resourceFile = Join-Path $outputDirectory 'ColorTool.Resources.resources'
& $resgen (Join-Path $projectDirectory 'Resources.resx') $resourceFile
if ($LASTEXITCODE) { exit $LASTEXITCODE }

$references = Get-ChildItem $frameworkDirectory -Filter *.dll | Where-Object {
    try {
        [Reflection.AssemblyName]::GetAssemblyName($_.FullName) | Out-Null
        $true
    } catch {
        $false
    }
} | ForEach-Object { "/reference:$($_.FullName)" }

$sources = Get-ChildItem $projectDirectory -Recurse -Filter *.cs | ForEach-Object FullName
$output = Join-Path $outputDirectory 'ColorTool.exe'
& $dotnet $csc /noconfig /nostdlib /target:exe "/out:$output" "/resource:$resourceFile,ColorTool.Resources.resources" /langversion:latest /platform:anycpu $references $sources
if ($LASTEXITCODE) { exit $LASTEXITCODE }

Copy-Item (Join-Path $projectDirectory 'App.config') "$output.config" -Force
Copy-Item $output (Join-Path $PSScriptRoot 'ColorTool.exe') -Force
Write-Host "Created $output and copied ColorTool.exe to $PSScriptRoot"
