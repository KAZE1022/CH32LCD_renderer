# gen_gb2312_uni.ps1 - 生成 Unicode <-> HZK16(GB2312) 映射表
#
#   HZK16/HZK16S 字库布局: 每字 32 字节
#       字符索引 index = (zone-0xA1)*94 + (pos-0xA1)
#   GB2312 汉字区: zone 0xB0..0xF7, pos 0xA1..0xFE
#       第 1 个汉字(啊 0xB0A1) 索引 = 1410, 汉字总数 = 6763
#
#   GB2312 顺序与 Unicode 码点不同序, 因此生成两张表:
#     1) Font_HZK_Unicode[] : 按字库顺序的 Unicode 码点 (13.5KB)
#     2) Font_HZK_SortedUni[] / Font_HZK_SortedMap[] : 按 Unicode 升序的
#        码点与对应字库位置 (各 13.5KB), 用 16 个粗桶 + 二分查找定位
#
# 输出: LCD\font_gb2312_map.c
param(
    [string]$OutFile = "$PSScriptRoot\..\LCD\font_gb2312_map.c"
)

$pc = [System.Text.Encoding]::GetEncoding(936)

$count = 6763
$uni = New-Object 'System.UInt16[]' $count
$n = 0
for ($zone = 0xB0; $zone -le 0xF7; $zone++) {
    for ($pos = 0xA1; $pos -le 0xFE; $pos++) {
        $gb = [byte[]]@([byte]$zone, [byte]$pos)
        $s = $pc.GetString($gb)
        if ($s.Length -eq 1) {
            $u = [int][char]$s
            if ($u -ge 0x4E00 -and $u -le 0x9FA5) {
                $uni[$n] = [uint16]$u
                $n++
            }
        }
    }
}
if ($n -ne $count) { throw "汉字数 $n != $count" }
Write-Host ("汉字数 = {0}" -f $n)

# 排序: 先在 16 个大区(高 12 位)内按 Unicode 升序排, 保证每个大区内
# 排序后仍然按字库顺序连续(便于用区间表 + 二分), 再生成查找表。
$sorted = New-Object 'System.UInt16[]' $count
$map    = New-Object 'System.UInt16[]' $count
$pos = 0
for ($g = 0; $g -lt 16; $g++) {
    $lo = $g * 4096
    $hi = $lo + 4095
    $items = New-Object System.Collections.Generic.List[object]
    for ($i = 0; $i -lt $n; $i++) {
        $u = [int]$uni[$i]
        if ($u -ge $lo -and $u -le $hi) { $items.Add([pscustomobject]@{ U = $u; I = $i }) }
    }
    $items = $items | Sort-Object U
    foreach ($it in $items) {
        $sorted[$pos] = [uint16]$it.U
        $map[$pos] = [uint16]$it.I
        $pos++
    }
}
if ($pos -ne $count) { throw "排序后数量 $pos != $count" }

# 大区表: group[g] = 第 g 个大区在 sorted[] 中的起始位置
$group = New-Object 'System.Int32[]' 17
$k = 0
for ($g = 0; $g -lt 16; $g++) {
    $group[$g] = $k
    while ($k -lt $count -and ([int]$sorted[$k]) -lt (($g + 1) * 4096)) { $k++ }
}
$group[16] = $k
$maxG = 0
for ($g = 0; $g -lt 16; $g++) { $c = $group[$g + 1] - $group[$g]; if ($c -gt $maxG) { $maxG = $c } }
Write-Host ("最大大区大小 = {0} (二分深度约 {1})" -f $maxG, [Math]::Ceiling([Math]::Log($maxG, 2)))

# 自检: 大字区内按 Unicode 升序
$sortedOk = $true
for ($g = 0; $g -lt 16; $g++) {
    for ($i = $group[$g] + 1; $i -lt $group[$g + 1]; $i++) {
        if ([int]$sorted[$i] -le [int]$sorted[$i - 1]) { $sortedOk = $false }
    }
}
Write-Host ("大区内 Unicode 升序 = {0}" -f $sortedOk)

function Test-Lookup([int]$u) {
    $g = $u -shr 12
    $lo = $group[$g]
    $hi = $group[$g + 1] - 1
    while ($lo -le $hi) {
        $mid = [int](($lo + $hi) -shr 1)
        $v = [int]$sorted[$mid]
        if ($v -eq $u) { return [int]$map[$mid] }
        if ($v -lt $u) { $lo = $mid + 1 } else { $hi = $mid - 1 }
    }
    return -1
}
$bad = 0
for ($i = 0; $i -lt $count; $i++) {
    if ((Test-Lookup ([int]$uni[$i])) -ne $i) {
        $bad++
        if ($bad -le 5) { Write-Host ("查找失败: index={0} uni=0x{1:X4}" -f $i, $uni[$i]) }
    }
}
Write-Host ("查找自检失败数 = {0}" -f $bad)
if ($bad -ne 0) { throw "查找表自检失败" }

$sb = New-Object System.Text.StringBuilder
[void]$sb.AppendLine("/* 本文件由 tools/gen_gb2312_uni.ps1 自动生成，请勿手工修改。")
[void]$sb.AppendLine(" *")
[void]$sb.AppendLine(" * 用途: 当源文件是 UTF-8 编码时, 把字符串里的 Unicode 汉字翻译成")
[void]$sb.AppendLine(" *       HZK16 字库里的位置。源文件若是 ANSI/GB2312 编码则不需要查表。")
[void]$sb.AppendLine(" */")
[void]$sb.AppendLine("#include ""font.h""")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("/* 第 1 个汉字(啊 0xB0A1)在字库里的字符索引 */")
[void]$sb.AppendLine("#define HZ_FIRST_CHAR_INDEX   (1410u)")
[void]$sb.AppendLine("#define HZ_CHAR_COUNT         ($($count)u)")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("const uint32_t Font_HZK_FirstCharIndex = HZ_FIRST_CHAR_INDEX;")
[void]$sb.AppendLine("const uint32_t Font_HZK_CharCount      = HZ_CHAR_COUNT;")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("/* 按字库(GB2312)顺序排列的 Unicode 码点 */")
[void]$sb.AppendLine("const uint16_t Font_HZK_Unicode[HZ_CHAR_COUNT] = {")
for ($i = 0; $i -lt $count; $i += 12) {
    $end = [Math]::Min($i + 11, $count - 1)
    $parts = @()
    for ($j = $i; $j -le $end; $j++) { $parts += ("0x{0:X4}" -f $uni[$j]) }
    [void]$sb.AppendLine("    " + ($parts -join ",") + ",")
}
[void]$sb.AppendLine("};")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("/* 按 Unicode 升序的分区表: 第 g 区 = 码点 [g*4096, g*4096+4095] */")
[void]$sb.AppendLine("const uint16_t Font_HZK_SortedGroup[17] = {")
$parts = @()
for ($g = 0; $g -le 16; $g++) { $parts += ("$($group[$g])u") }
[void]$sb.AppendLine("    " + ($parts -join ",") + ",")
[void]$sb.AppendLine("};")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("/* 按 Unicode 升序的码点 */")
[void]$sb.AppendLine("const uint16_t Font_HZK_SortedUni[HZ_CHAR_COUNT] = {")
for ($i = 0; $i -lt $count; $i += 12) {
    $end = [Math]::Min($i + 11, $count - 1)
    $parts = @()
    for ($j = $i; $j -le $end; $j++) { $parts += ("0x{0:X4}" -f $sorted[$j]) }
    [void]$sb.AppendLine("    " + ($parts -join ",") + ",")
}
[void]$sb.AppendLine("};")
[void]$sb.AppendLine("")
[void]$sb.AppendLine("/* 对应的字库字符索引 */")
[void]$sb.AppendLine("const uint16_t Font_HZK_SortedMap[HZ_CHAR_COUNT] = {")
for ($i = 0; $i -lt $count; $i += 12) {
    $end = [Math]::Min($i + 11, $count - 1)
    $parts = @()
    for ($j = $i; $j -le $end; $j++) { $parts += ("$($map[$j])u") }
    [void]$sb.AppendLine("    " + ($parts -join ",") + ",")
}
[void]$sb.AppendLine("};")

[System.IO.File]::WriteAllText($OutFile, $sb.ToString(), (New-Object System.Text.UTF8Encoding($false)))
Write-Host ("written {0} ({1} bytes)" -f $OutFile, (Get-Item $OutFile).Length)
