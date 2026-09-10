param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath,
    [Parameter(Mandatory = $true)]
    [string]$OutputPath
    ,
    [Parameter(Mandatory = $true)]
    [string]$Version,
    [Parameter(Mandatory = $true)]
    [string]$AboutDialogPath
)

$text = [System.IO.File]::ReadAllText($InputPath, [System.Text.UTF8Encoding]::new($false))
$aboutDialog = [System.IO.File]::ReadAllText($AboutDialogPath, [System.Text.UTF8Encoding]::new($false))
$authorMatch = [regex]::Match($aboutDialog, '<Run\s+Text="(?<author>[^"<(]+?)\s*\((?<email>[^)\"]+)\)"\s*/>')

if (-not $authorMatch.Success) {
    throw "Could not read author and email from $AboutDialogPath."
}

$author = $authorMatch.Groups['author'].Value.Trim()
$email = $authorMatch.Groups['email'].Value.Trim()
$text = $text.Replace('{{VERSION}}', $Version).Replace('{{AUTHOR}}', $author).Replace('{{EMAIL}}', $email)
$rtf = [System.Text.StringBuilder]::new('{\rtf1\ansi\deff0{\fonttbl{\f0 Segoe UI;}}\viewkind4\uc1\pard\sa120\sl276\slmult1\f0\fs22 ')

function Add-RtfText([System.Text.StringBuilder]$Builder, [string]$Value) {
    foreach ($character in $Value.ToCharArray()) {
        switch ([int][char]$character) {
            92 { [void]$Builder.Append('\\') } # backslash
            123 { [void]$Builder.Append('\{') } # opening brace
            125 { [void]$Builder.Append('\}') } # closing brace
            default {
                $codePoint = [int][char]$character
                if ($codePoint -le 127) {
                    [void]$Builder.Append($character)
                } else {
                    [void]$Builder.Append("\u$codePoint?")
                }
            }
        }
    }
}

$lines = $text -split "`r?`n"
$firstContentLine = $true
foreach ($line in $lines) {
    if ([string]::IsNullOrWhiteSpace($line)) {
        [void]$rtf.Append('\par ')
        continue
    }

    if ($firstContentLine) {
        [void]$rtf.Append('\qc\b\fs34 ')
        Add-RtfText $rtf $line
        [void]$rtf.Append('\par\b0\fs22\sa40 ')
        $firstContentLine = $false
        continue
    }

    if ($line -match '^[0-9]+\.\s+') {
        [void]$rtf.Append('\b\sa80 ')
        Add-RtfText $rtf $line
        [void]$rtf.Append('\b0\sa120\par ')
        continue
    }

    Add-RtfText $rtf $line
    [void]$rtf.Append('\par ')
}

[void]$rtf.Append('}')
[System.IO.File]::WriteAllText($OutputPath, $rtf.ToString(), [System.Text.ASCIIEncoding]::new())
