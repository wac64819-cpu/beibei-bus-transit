# Tile downloader for Beibei District (English-only to avoid encoding issues)

$ErrorActionPreference = "Continue"
$tilesDir = Join-Path $PSScriptRoot "tiles"
New-Item -ItemType Directory -Path $tilesDir -Force | Out-Null

$LAT_MIN = 29.70; $LAT_MAX = 29.88
$LNG_MIN = 106.35; $LNG_MAX = 106.52

function Deg2Tile($lat, $lon, $zoom) {
    $n = [math]::Pow(2, $zoom)
    $x = [int][math]::Floor(($lon + 180.0) / 360.0 * $n)
    $latRad = $lat * [math]::PI / 180.0
    $y = [int][math]::Floor((1.0 - [math]::Log([math]::Tan($latRad) + 1.0 / [math]::Cos($latRad)) / [math]::PI) / 2.0 * $n)
    return @($x, $y)
}

$totalCount = 0; $okCount = 0; $skipCount = 0; $failCount = 0
$startTime = Get-Date

$urlTpl = 'https://webrd0{0}.is.autonavi.com/appmaptile?lang=zh_cn{1}size=1{1}scale=1{1}style=8{1}x={2}{1}y={3}{1}z={4}'
$amp = [char]38

Write-Host "=== Tile Downloader ==="
Write-Host "Output dir: $tilesDir"

foreach ($zoom in 12..16) {
    $tl = Deg2Tile $LAT_MAX $LNG_MIN $zoom
    $br = Deg2Tile $LAT_MIN $LNG_MAX $zoom
    $x1 = $tl[0]; $y1 = $tl[1]
    $x2 = $br[0]; $y2 = $br[1]
    $count = ($x2 - $x1 + 1) * ($y2 - $y1 + 1)
    Write-Host "Zoom z=$zoom : x=$x1..$x2, y=$y1..$y2  total $count tiles"

    for ($x = $x1; $x -le $x2; $x++) {
        for ($y = $y1; $y -le $y2; $y++) {
            $totalCount++
            $outDir = Join-Path $tilesDir "$zoom\$x"
            $outFile = Join-Path $outDir "$y.png"

            if (Test-Path $outFile) { $skipCount++; continue }
            New-Item -ItemType Directory -Path $outDir -Force | Out-Null

            $sub = ($totalCount % 4) + 1
            $url = $urlTpl -f $sub, $amp, $x, $y, $zoom

            try {
                Invoke-WebRequest -Uri $url -OutFile $outFile -TimeoutSec 10 -UseBasicParsing -ErrorAction Stop
                $okCount++
                if ($okCount % 30 -eq 0) {
                    $elapsed = (Get-Date) - $startTime
                    Write-Host ("  Progress: ok={0} skip={1} fail={2} time={3:mm\:ss}" -f $okCount, $skipCount, $failCount, $elapsed)
                }
            } catch {
                $failCount++
                if ($failCount -le 3) { Write-Host ("  FAIL z={0} x={1} y={2}" -f $zoom, $x, $y) }
                if (Test-Path $outFile) { Remove-Item $outFile -Force }
            }
        }
    }
}

$elapsed = (Get-Date) - $startTime
$totalSize = (Get-ChildItem $tilesDir -Recurse -File -ErrorAction SilentlyContinue | Measure-Object Length -Sum).Sum
Write-Host ""
Write-Host "=== Done ==="
Write-Host ("Total : {0}" -f $totalCount)
Write-Host ("OK    : {0}" -f $okCount)
Write-Host ("Skip  : {0}" -f $skipCount)
Write-Host ("Fail  : {0}" -f $failCount)
Write-Host ("Time  : {0:mm\:ss}" -f $elapsed)
Write-Host ("Size  : {0:N1} MB" -f ($totalSize / 1MB))
