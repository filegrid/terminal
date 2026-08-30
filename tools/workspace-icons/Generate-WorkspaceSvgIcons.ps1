param(
    [Parameter(Mandatory = $true)]
    [string]$OutputRoot
)

$ErrorActionPreference = 'Stop'

$utf8NoBom = [System.Text.UTF8Encoding]::new($false)

function Write-Utf8NoBom {
    param(
        [string]$Path,
        [string]$Content
    )

    $dir = Split-Path -Parent $Path
    if (-not [string]::IsNullOrWhiteSpace($dir)) {
        New-Item -ItemType Directory -Force -Path $dir | Out-Null
    }

    [System.IO.File]::WriteAllText($Path, $Content, $utf8NoBom)
}

function New-IconSvg {
    param(
        [string]$Family,
        [string]$Glyph,
        [string]$Accent,
        [string]$Accent2
    )

    $escapedGlyph = [System.Security.SecurityElement]::Escape($Glyph)

    switch ($Family) {
        'color' {
            return @"
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64">
  <rect x="6" y="6" width="52" height="52" rx="16" fill="$Accent"/>
  <circle cx="48" cy="16" r="6" fill="$Accent2" opacity="0.92"/>
  <text x="32" y="40" text-anchor="middle" font-family="Segoe UI Symbol, Segoe UI" font-size="24" fill="#ffffff">$escapedGlyph</text>
</svg>
"@
        }
        'outline' {
            return @"
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64">
  <rect x="7" y="7" width="50" height="50" rx="15" fill="#182331" stroke="$Accent" stroke-width="3"/>
  <text x="32" y="40" text-anchor="middle" font-family="Segoe UI Symbol, Segoe UI" font-size="24" fill="$Accent">$escapedGlyph</text>
</svg>
"@
        }
        'solid' {
            return @"
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64">
  <rect x="5" y="5" width="54" height="54" rx="13" fill="#10161d"/>
  <rect x="10" y="10" width="44" height="44" rx="10" fill="$Accent"/>
  <text x="32" y="40" text-anchor="middle" font-family="Segoe UI Symbol, Segoe UI" font-size="24" fill="#ffffff">$escapedGlyph</text>
</svg>
"@
        }
        'rounded' {
            return @"
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64">
  <rect x="4" y="4" width="56" height="56" rx="20" fill="#f3f7ff"/>
  <rect x="10" y="10" width="44" height="44" rx="16" fill="$Accent" opacity="0.18"/>
  <text x="32" y="40" text-anchor="middle" font-family="Segoe UI Symbol, Segoe UI" font-size="24" fill="$Accent">$escapedGlyph</text>
</svg>
"@
        }
        'duotone' {
            return @"
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64">
  <rect x="6" y="6" width="52" height="52" rx="16" fill="$Accent" opacity="0.2"/>
  <path d="M6 32c0-14.36 11.64-26 26-26h0c14.36 0 26 11.64 26 26v10c0 8.84-7.16 16-16 16H22C13.16 58 6 50.84 6 42V32z" fill="$Accent2" opacity="0.92"/>
  <text x="32" y="40" text-anchor="middle" font-family="Segoe UI Symbol, Segoe UI" font-size="24" fill="#ffffff">$escapedGlyph</text>
</svg>
"@
        }
        'sharp' {
            return @"
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64">
  <rect x="7" y="7" width="50" height="50" fill="#111827"/>
  <rect x="11" y="11" width="42" height="42" fill="$Accent"/>
  <path d="M11 11h18l-18 18z" fill="$Accent2" opacity="0.75"/>
  <text x="32" y="40" text-anchor="middle" font-family="Segoe UI Symbol, Segoe UI" font-size="24" fill="#ffffff">$escapedGlyph</text>
</svg>
"@
        }
        default {
            throw "Unknown family: $Family"
        }
    }
}

$families = @(
    @{ Id = 'color'; Accent = '#2463eb'; Accent2 = '#4db6ff' },
    @{ Id = 'outline'; Accent = '#5ba7ff'; Accent2 = '#8dd0ff' },
    @{ Id = 'solid'; Accent = '#2f7df4'; Accent2 = '#63b3ff' },
    @{ Id = 'rounded'; Accent = '#377dff'; Accent2 = '#8ac7ff' },
    @{ Id = 'duotone'; Accent = '#2463eb'; Accent2 = '#6e49cb' },
    @{ Id = 'sharp'; Accent = '#1d4ed8'; Accent2 = '#60a5fa' }
)

function Glyph([int]$CodePoint) {
    return [string][char]$CodePoint
}

$numbers = 0..9 | ForEach-Object { [string]$_ }
$letters = @()
for ($i = [byte][char]'A'; $i -le [byte][char]'Z'; $i++) {
    $letters += [string][char]$i
}

$daily = @(
    (Glyph 0x2302), (Glyph 0x2600), (Glyph 0x2601), (Glyph 0x2602), (Glyph 0x260E),
    (Glyph 0x2709), (Glyph 0x2315), (Glyph 0x2661), (Glyph 0x2605), (Glyph 0x266A),
    (Glyph 0x2691), (Glyph 0x2708), (Glyph 0x231A), (Glyph 0x2302), (Glyph 0x265F),
    (Glyph 0x2615), (Glyph 0x2699), (Glyph 0x2301), (Glyph 0x2637), (Glyph 0x2667)
)
$development = @(
    '</>', '{}', '[]', '()', '#',
    '@', '$', '%', '+', '=',
    (Glyph 0x2197), (Glyph 0x2318), (Glyph 0x2699), (Glyph 0x2318), (Glyph 0x2317),
    (Glyph 0x2301), (Glyph 0x2302), (Glyph 0x2387), (Glyph 0x232C), (Glyph 0x2328)
)
$office = @(
    (Glyph 0x25A3), (Glyph 0x25A4), (Glyph 0x25A5), (Glyph 0x25A6), (Glyph 0x25A7),
    (Glyph 0x25A8), (Glyph 0x25A9), (Glyph 0x270E), (Glyph 0x2713), (Glyph 0x2717),
    (Glyph 0x2611), (Glyph 0x2612), (Glyph 0x2637), (Glyph 0x2630), (Glyph 0x2637),
    (Glyph 0x2301), (Glyph 0x2709), (Glyph 0x25A4), (Glyph 0x25A5), (Glyph 0x25A6)
)
$windows = @(
    (Glyph 0x229E), (Glyph 0x25A6), (Glyph 0x25A3), (Glyph 0x25A4), (Glyph 0x25A5),
    (Glyph 0x25A7), (Glyph 0x25A8), (Glyph 0x25A9), (Glyph 0x25EB), (Glyph 0x25E7),
    (Glyph 0x25E9), (Glyph 0x25EA), (Glyph 0x25F0), (Glyph 0x25F1), (Glyph 0x25F2),
    (Glyph 0x25F3), (Glyph 0x25F4), (Glyph 0x25F5), (Glyph 0x25F6), (Glyph 0x25F7)
)

$sections = @(
    @{ Name = 'numbers'; Glyphs = $numbers },
    @{ Name = 'letters'; Glyphs = $letters },
    @{ Name = 'daily'; Glyphs = $daily },
    @{ Name = 'development'; Glyphs = $development },
    @{ Name = 'office'; Glyphs = $office },
    @{ Name = 'windows'; Glyphs = $windows }
)

New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null

foreach ($family in $families) {
    foreach ($section in $sections) {
        for ($i = 0; $i -lt $section.Glyphs.Count; $i++) {
            $glyph = $section.Glyphs[$i]
            $path = Join-Path $OutputRoot (Join-Path $family.Id (Join-Path $section.Name "$i.svg"))
            $svg = New-IconSvg -Family $family.Id -Glyph $glyph -Accent $family.Accent -Accent2 $family.Accent2
            Write-Utf8NoBom -Path $path -Content $svg
        }
    }
}
