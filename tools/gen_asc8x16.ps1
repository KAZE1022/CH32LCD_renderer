param(
    [string]$FontName = "Consolas",
    [int]$FontPixel = 14,
    [int]$BaselineRow = 12,
    [string]$OutFile = "$PSScriptRoot\asc8x16.bin"
)
Add-Type -AssemblyName System.Drawing
$pc = [System.Text.Encoding]::GetEncoding(936)
$canvas = 64
$bmp = New-Object System.Drawing.Bitmap $canvas, $canvas, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::SingleBitPerPixelGridFit
$font = New-Object System.Drawing.Font($FontName, $FontPixel, [System.Drawing.FontStyle]::Regular, [System.Drawing.GraphicsUnit]::Pixel)

$count = 95            # 0x20..0x7E
$buf = New-Object 'byte[]' ($count * 16)
$idx = 0
for ($code = 0x20; $code -le 0x7E; $code++) {
    $ch = [char]$code
    $g.Clear([System.Drawing.Color]::Black)
    $fmt = New-Object System.Drawing.StringFormat
    $fmt.Alignment = [System.Drawing.StringAlignment]::Near
    $fmt.LineAlignment = [System.Drawing.StringAlignment]::Near
    $g.DrawString($ch.ToString(), $font, [System.Drawing.Brushes]::White, 0, 0, $fmt)
    $fmt.Dispose()
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
    if ($maxX -lt 0) { $idx++; continue }
    $w = $maxX - $minX + 1
    $h = $maxY - $minY + 1
    $ox = [int][Math]::Floor((8 - $w) / 2.0)
    if ($w -gt 8) { $ox = 0 }
    $oy = $BaselineRow - $maxY      # 让基线落在 BaselineRow
    if ($oy + $h - 1 -gt 15) { $oy = 15 - $h + 1 }
    if ($oy -lt 0) { $oy = 0 }
    $grid = New-Object 'byte[,]' 16, 8
    for ($y = 0; $y -lt $h; $y++) {
        $ty = $oy + $y
        if ($ty -lt 0 -or $ty -gt 15) { continue }
        for ($x = 0; $x -lt $w; $x++) {
            $tx = $ox + $x
            if ($tx -lt 0 -or $tx -gt 7) { continue }
            $p = $bmp.GetPixel($minX + $x, $minY + $y)
            if (($p.R + $p.G + $p.B) -gt 384) { $grid[$ty, $tx] = 1 }
        }
    }
    for ($row = 0; $row -lt 16; $row++) {
        $v = 0
        for ($col = 0; $col -lt 8; $col++) { if ($grid[$row, $col] -ne 0) { $v = $v -bor (0x80 -shr $col) } }
        $buf[$idx * 16 + $row] = [byte]$v
    }
    $idx++
}
$g.Dispose(); $bmp.Dispose(); $font.Dispose()
[System.IO.File]::WriteAllBytes($OutFile, $buf)
Write-Host ("{0} -> {1} bytes" -f $OutFile, $buf.Length)

foreach ($t in @("A", "g", "W", "1", "@", "j")) {
    $off = ([int][char]$t - 0x20) * 16
    Write-Host "--- '$t'"
    for ($row = 0; $row -lt 16; $row++) {
        $line = ""
        $v = $buf[$off + $row]
        for ($bit = 7; $bit -ge 0; $bit--) { $line += $(if ($v -band (1 -shl $bit)) { "#" } else { "." }) }
        Write-Host $line
    }
}
