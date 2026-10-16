# bin2c.ps1 - 把二进制文件转换成 C 数组源文件
param(
    [Parameter(Mandatory=$true)][string]$InFile,
    [Parameter(Mandatory=$true)][string]$OutFile,
    [Parameter(Mandatory=$true)][string]$Symbol,
    [int]$BytesPerLine = 16
)
$bytes = [System.IO.File]::ReadAllBytes($InFile)
$sb = New-Object System.Text.StringBuilder
[void]$sb.AppendLine("/* 本文件由 tools/bin2c.ps1 自动生成，请勿手工修改 */")
[void]$sb.AppendLine("#include ""font_hz16.h""")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("const unsigned char $Symbol[$($bytes.Length)] = {")
for ($i = 0; $i -lt $bytes.Length; $i += $BytesPerLine) {
    $end = [Math]::Min($i + $BytesPerLine, $bytes.Length) - 1
    $parts = @()
    for ($j = $i; $j -le $end; $j++) { $parts += ("0x{0:X2}" -f $bytes[$j]) }
    [void]$sb.AppendLine("    " + ($parts -join ",") + ",")
}
[void]$sb.AppendLine("};")
[System.IO.File]::WriteAllText($OutFile, $sb.ToString(), (New-Object System.Text.UTF8Encoding($false)))
Write-Host ("{0} -> {1} ({2} bytes data, {3} bytes file)" -f $InFile, $OutFile, $bytes.Length, (Get-Item $OutFile).Length)
