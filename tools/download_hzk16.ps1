# download_hzk16.ps1 - 下载标准 HZK16(GB2312 16x16) 点阵字库
#
#   标准 HZK16 文件大小 267616 字节, 格式与生成出来的 hzk16.bin 完全一致:
#       每个字符 32 字节, 逐行 2 字节, 高位在左
#       位置 = ((区码-0xA1)*94 + (位码-0xA1)) * 32
#
#   用法:
#       powershell -ExecutionPolicy Bypass -File tools\download_hzk16.ps1
#       powershell -ExecutionPolicy Bypass -File tools\download_hzk16.ps1 -Url <网址>
#
#   下载完再转成 C 数组:
#       powershell -ExecutionPolicy Bypass -File tools\bin2c.ps1 `
#           -InFile tools\hzk16.bin -OutFile LCD\font_hz16.c -Symbol HZK16_FontData
param(
    [string[]]$Url = @(
        'https://cdn.jsdelivr.net/gh/larme/BitmapFont@master/font/HZK16S',
        'https://cdn.jsdelivr.net/gh/larme/BitmapFont@master/font/HZK16',
        'https://cdn.jsdelivr.net/gh/ProfFan/BitmapFont@master/font/HZK16',
        'https://cdn.jsdelivr.net/gh/m5stack/m5-hzk16@master/HZK16/Fonts/HZK16'
    ),
    [string]$OutFile = "$PSScriptRoot\hzk16.bin"
)

$ok = $false
foreach ($u in $Url) {
    Write-Host "尝试下载: $u"
    try {
        $req = [System.Net.WebRequest]::Create($u)
        $req.Timeout = 30000
        $req.UserAgent = 'Mozilla/5.0'
        $resp = $req.GetResponse()
        $ms = New-Object System.IO.MemoryStream
        $resp.GetResponseStream().CopyTo($ms)
        $bytes = $ms.ToArray()
        $ms.Dispose(); $resp.Close()

        Write-Host ("  收到 {0} 字节" -f $bytes.Length)
        if ($bytes.Length -ge 260000) {
            [System.IO.File]::WriteAllBytes($OutFile, $bytes)
            Write-Host ("  已保存: {0}" -f $OutFile)
            $ok = $true
            break
        }
        Write-Host "  文件太小, 不是完整字库, 试下一个地址"
    }
    catch {
        Write-Host ("  失败: {0}" -f $_.Exception.Message)
    }
}

if (-not $ok) {
    Write-Host ""
    Write-Host "全部地址都失败。可以改用本地方式生成字库:"
    Write-Host "  powershell -ExecutionPolicy Bypass -File tools\gen_hzk16.ps1 -Preview"
    exit 1
}

Write-Host ""
Write-Host "转换成本文件里的 C 数组:"
Write-Host "  powershell -ExecutionPolicy Bypass -File tools\bin2c.ps1 -InFile tools\hzk16.bin -OutFile LCD\font_hz16.c -Symbol HZK16_FontData"
