$ErrorActionPreference = 'Continue'
$gcc = 'D:\MounRiver_studio_setup\MounRiver_Studio2\resources\app\resources\win32\components\WCH\Toolchain\RISC-V Embedded GCC\bin\riscv-none-embed-gcc.exe'
$sdk = 'D:\baidudownload\CH32V307EVT\EVT\EXAM\SRC'
$inc = @("$sdk\Core", "$sdk\Peripheral\inc", "$sdk\Debug", "$PWD\LCD", "$PWD\User", "$sdk\Startup")
$idir = $inc | ForEach-Object { "-I$_" }
$out = "$PWD\obj\linkcheck"
New-Item -ItemType Directory -Force -Path $out | Out-Null

$cflags = @('-march=rv32imac', '-mabi=ilp32', '-msmall-data-limit=8', '-msave-restore', '-Os',
            '-fmessage-length=0', '-fsigned-char', '-ffunction-sections', '-fdata-sections',
            '-fno-common', '-std=gnu99')

$files = @("$PWD\LCD\spi_soft.c", "$PWD\LCD\lcd.c", "$PWD\User\main.c",
           "$PWD\User\ch32v30x_it.c", "$PWD\User\system_ch32v30x.c")
$objs = @()
$errs = @()

foreach ($f in $files) {
    $obj = Join-Path $out ((Split-Path $f -Leaf) + '.o')
    $r = & $gcc @cflags @idir -c $f -o $obj 2>&1
    if ($LASTEXITCODE -ne 0) { $errs += "CC FAIL $f`n" + ($r | Out-String) } else { $objs += $obj }
}

$asm = "$sdk\Startup\startup_ch32v30x_D8C.S"
$ao = Join-Path $out 'startup.o'
$r = & $gcc @cflags @idir -c $asm -o $ao 2>&1
if ($LASTEXITCODE -ne 0) { $errs += "AS FAIL`n" + ($r | Out-String) } else { $objs += $ao }

$libs = @('ch32v30x_gpio.c', 'ch32v30x_rcc.c', 'ch32v30x_usart.c', 'ch32v30x_spi.c',
          'ch32v30x_dma.c', 'ch32v30x_misc.c', 'ch32v30x_tim.c', 'ch32v30x_flash.c',
          'ch32v30x_exti.c', 'ch32v30x_pwr.c', 'ch32v30x_adc.c')
foreach ($l in $libs) {
    $src = "$sdk\Peripheral\src\$l"
    if (Test-Path $src) {
        $obj = Join-Path $out ($l + '.o')
        $r = & $gcc @cflags @idir -c $src -o $obj 2>&1
        if ($LASTEXITCODE -ne 0) { $errs += "CC FAIL $l`n" + ($r | Out-String) } else { $objs += $obj }
    }
}

$dbg = "$sdk\Debug\debug.c"
if (Test-Path $dbg) {
    $obj = Join-Path $out 'debug.o'
    $r = & $gcc @cflags @idir -c $dbg -o $obj 2>&1
    if ($LASTEXITCODE -eq 0) { $objs += $obj } else { $errs += "CC FAIL debug.c`n" + ($r | Out-String) }
}

Write-Output ("objs = {0}" -f $objs.Count)

if ($errs.Count) {
    Write-Output ($errs -join "`n")
    exit 1
}

$elf = Join-Path $out 'SPI_LCD.elf'
$linkArgs = @('-T', "$sdk\Ld\Link.ld", '-Wl,--gc-sections', '-Wl,--print-memory-usage', '-nostartfiles', '-o', $elf) + $objs + @('-lc', '-lnosys', '-lgcc')
$r = & $gcc @cflags @linkArgs 2>&1
Write-Output ("LINK exit = {0}" -f $LASTEXITCODE)
Write-Output (($r | Out-String).Trim())

if ($LASTEXITCODE -eq 0) {
    $size = 'D:\MounRiver_studio_setup\MounRiver_Studio2\resources\app\resources\win32\components\WCH\Toolchain\RISC-V Embedded GCC\bin\riscv-none-embed-size.exe'
    Write-Output '--- size ---'
    & $size $elf
}
