param(
    [string]$FontName = "SimHei",
    [int]$FontPixel = 16,
    [string]$OutFile = "$PSScriptRoot\hzk16.bin",
    [switch]$Preview,
    [switch]$NoWorkaround
)

Add-Type -AssemblyName System.Drawing

$zoneStart = 0xA1
$zoneEnd   = 0xF7
$maxPos    = 0xFE
$zones     = $zoneEnd - $zoneStart + 1
$total     = $zones * 94
$bytesPerChar = 32
Write-Host ("zones={0} total={1} bytes={2}" -f $zones, $total, $total * $bytesPerChar)

$pc = [System.Text.Encoding]::GetEncoding(936)
$canvas = 48
$bmp = New-Object System.Drawing.Bitmap $canvas, $canvas, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::SingleBitPerPixelGridFit
$g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::None
$font = New-Object System.Drawing.Font($FontName, $FontPixel, [System.Drawing.FontStyle]::Regular, [System.Drawing.GraphicsUnit]::Pixel)
Write-Host ("font={0} px={1} gdiHeight={2}" -f $font.Name, $FontPixel, $font.Height)

$grid = New-Object 'byte[,]' 16, 16
$buf  = New-Object 'byte[]' ($total * $bytesPerChar)

function Test-Glyph([string]$ch) {
    $bytes = $pc.GetBytes($ch)
    if ($bytes.Length -ne 2) { return $false }
    if ($bytes[0] -lt $zoneStart -or $bytes[0] -gt $zoneEnd) { return $false }
    if ($bytes[1] -lt 0xA1 -or $bytes[1] -gt $maxPos) { return $false }
    return $true
}

$idx = 0
for ($zone = $zoneStart; $zone -le $zoneEnd; $zone++) {
    for ($pos = 0xA1; $pos -le $maxPos; $pos++) {
        $gb = [byte[]]@([byte]$zone, [byte]$pos)
        $ch = $pc.GetString($gb)
        $base = $idx * $bytesPerChar
        $idx++
        if (-not (Test-Glyph $ch)) { continue }
        $back = $pc.GetBytes($ch)
        if ($back[0] -ne $zone -or $back[1] -ne $pos) { continue }

        $g.Clear([System.Drawing.Color]::Black)
        $fmt = New-Object System.Drawing.StringFormat
        $fmt.Alignment = [System.Drawing.StringAlignment]::Near
        $fmt.LineAlignment = [System.Drawing.StringAlignment]::Near
        $g.DrawString($ch, $font, [System.Drawing.Brushes]::White, 0, 0, $fmt)
        $fmt.Dispose()

        # 找墨迹包围盒
        $minX = 99; $maxX = -1; $minY = 99; $maxY = -1
        for ($y = 0; $y -lt $canvas; $y++) {
            for ($x = 0; $x -lt $canvas; $x++) {
                $p = $bmp.GetPixel($x, $y)
                if (($p.R + $p.G + $p.B) -gt 384) {
                    if ($x -lt $minX) { $minX = $x }
                    if ($x -gt $maxX) { $maxX = $x }
                    if ($y -lt $minY) { $minY = $y }
                    if ($y -gt $maxY) { $maxY = $y }
                }
            }
        }
        if ($maxX -lt 0) { continue }   # 空字形

        $w = $maxX - $minX + 1
        $h = $maxY - $minY + 1
        $ox = [int][Math]::Floor((16 - $w) / 2.0)
        $oy = [int][Math]::Floor((16 - $h) / 2.0)
        if ($w -gt 16) { $ox = 0 }
        if ($h -gt 16) { $oy = 0 }

        for ($i = 0; $i -lt 16; $i++) { for ($j = 0; $j -lt 16; $j++) { $grid[$i, $j] = 0 } }

        for ($y = 0; $y -lt $h; $y++) {
            $ty = $oy + $y
            if ($ty -lt 0 -or $ty -gt 15) { continue }
            for ($x = 0; $x -lt $w; $x++) {
                $tx = $ox + $x
                if ($tx -lt 0 -or $tx -gt 15) { continue }
                $p = $bmp.GetPixel($minX + $x, $minY + $y)
                if (($p.R + $p.G + $p.B) -gt 384) { $grid[$ty, $tx] = 1 }
            }
        }

        for ($row = 0; $row -lt 16; $row++) {
            $hi = 0; $lo = 0
            for ($col = 0; $col -lt 8; $col++)  { if ($grid[$row, $col] -ne 0) { $hi = $hi -bor (0x80 -shr $col) } }
            for ($col = 8; $col -lt 16; $col++) { if ($grid[$row, $col] -ne 0) { $lo = $lo -bor (0x80 -shr ($col - 8)) } }
            $buf[$base + $row * 2]     = [byte]$hi
            $buf[$base + $row * 2 + 1] = [byte]$lo
        }
    }
}
$g.Dispose(); $bmp.Dispose(); $font.Dispose()

[System.IO.File]::WriteAllBytes($OutFile, $buf)
Write-Host ("written {0} ({1} bytes)" -f $OutFile, (Get-Item $OutFile).Length)

if ($Preview) {
    foreach ($s in @("中", "文", "啊", "显", "示", "！")) {
        $bytes = $pc.GetBytes($s)
        $off = (($bytes[0] - 0xA1) * 94 + ($bytes[1] - 0xA1)) * 32
        Write-Host ("--- {0} 0x{1:X2}{2:X2} off={3}" -f $s, $bytes[0], $bytes[1], $off)
        for ($row = 0; $row -lt 16; $row++) {
            $line = ""
            for ($b = 0; $b -lt 2; $b++) {
                $v = $buf[$off + $row * 2 + $b]
                for ($bit = 7; $bit -ge 0; $bit--) { $line += $(if ($v -band (1 -shl $bit)) { "#" } else { "." }) }
            }
            Write-Host $line
        }
    }
}
