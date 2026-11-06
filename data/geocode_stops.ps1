Add-Type -AssemblyName System.Web
$key = $env:AMAP_API_KEY
$inFile = Join-Path $PSScriptRoot "stops.csv"
$outFile = Join-Path $PSScriptRoot "stops_geocoded.csv"
$bakFile = Join-Path $PSScriptRoot "stops_original.csv"
$reportFile = Join-Path $PSScriptRoot "geocode_report.txt"

if (-not (Test-Path $bakFile)) { Copy-Item $inFile $bakFile }

$lines = Get-Content $inFile -Encoding UTF8
$header = $lines[0]
$results = @($header)
$report = @()

$total = $lines.Count - 1
$idx = 0
$okBus = 0
$okGeneral = 0
$miss = 0

$adcode = "500109"  # Beibei district
$centerLng = 106.4373
$centerLat = 29.8058
$maxDistKm = 20.0  # max acceptable distance from Beibei center (in 0.01-deg units)

function Query-AMap($keywords, $types) {
    $kw = [System.Web.HttpUtility]::UrlEncode($keywords)
    $url = "https://restapi.amap.com/v3/place/text?keywords=" + $kw + "&city=500000&output=json&offset=15&extensions=base&key=" + $key
    if ($types) { $url += "&types=" + $types }
    try {
        $r = Invoke-WebRequest $url -TimeoutSec 8 -UseBasicParsing -ErrorAction Stop
        $j = $r.Content | ConvertFrom-Json
        if ($j.status -eq "1" -and $j.pois) { return $j.pois }
    } catch { }
    return @()
}

function Pick-Best($pois, $queryName) {
    $bestPoi = $null
    $bestScore = -1.0
    foreach ($poi in $pois) {
        # use numeric adcode (500109 = Beibei) instead of Chinese name comparison
        if ($poi.adcode -ne "500109") { continue }
        $loc = $poi.location -split ','
        if ($loc.Count -ne 2) { continue }
        $lng = [double]$loc[0]
        $lat = [double]$loc[1]
        $dlng = ($lng - $centerLng) * 100
        $dlat = ($lat - $centerLat) * 100
        $dist = [math]::Sqrt($dlng * $dlng + $dlat * $dlat)
        if ($dist -gt $maxDistKm) { continue }

        # Score: name match bonus + distance penalty
        $score = 10.0 - $dist * 0.1
        $cleanName = $poi.name -replace '\(.*\)', ''
        if ($cleanName -eq $queryName) { $score += 20.0 }
        elseif ($cleanName -like "*$queryName*") { $score += 10.0 }
        elseif ($queryName -like "*$cleanName*") { $score += 5.0 }

        if ($score -gt $bestScore) {
            $bestPoi = $poi
            $bestScore = $score
        }
    }
    return $bestPoi
}

foreach ($line in ($lines | Select-Object -Skip 1)) {
    $idx++
    $parts = $line -split ','
    if ($parts.Count -lt 4) { continue }
    $id = $parts[0]
    $name = $parts[1]
    $oldLng = $parts[2]
    $oldLat = $parts[3]

    $newLng = $oldLng
    $newLat = $oldLat
    $matchedName = ""
    $status = "MISS"

    # Beibei district prefix as unicode chars (avoid encoding issues)
    $beibeiPrefix = [char]0x5317 + [char]0x789A + [char]0x533A  # "BeibeiDistrict" in Chinese
    $beibeiShort = [char]0x5317 + [char]0x789A                   # "Beibei" in Chinese

    # Pass 1: search bus stops (types=150700) with Beibei prefix
    $pois = Query-AMap ($beibeiPrefix + $name) "150700"
    $best = Pick-Best $pois $name
    if ($best) {
        $loc = $best.location -split ','
        $newLng = $loc[0]; $newLat = $loc[1]
        $matchedName = $best.name
        $status = "BUS"
        $okBus++
    } else {
        # Pass 2: general POI search (no type filter)
        $pois2 = Query-AMap ($beibeiShort + $name) ""
        $best2 = Pick-Best $pois2 $name
        if ($best2) {
            $loc = $best2.location -split ','
            $newLng = $loc[0]; $newLat = $loc[1]
            $matchedName = $best2.name
            $status = "POI"
            $okGeneral++
        } else {
            $miss++
        }
    }

    $results += ($id + "," + $name + "," + $newLng + "," + $newLat)
    $report += ("[" + $status + "] id=" + $id + "  name=" + $name + "  old=(" + $oldLng + "," + $oldLat + ")  new=(" + $newLng + "," + $newLat + ")  matched=" + $matchedName)

    Write-Host ("[{0,3}/{1}] {2,-25} {3,-5}  ({4,11},{5,10})  {6}" -f $idx, $total, $name, $status, $newLng, $newLat, $matchedName)

    Start-Sleep -Milliseconds 130
}

$results | Out-File $outFile -Encoding UTF8
$report | Out-File $reportFile -Encoding UTF8

Write-Host ""
Write-Host "=== Done ==="
Write-Host ("Bus stop match: " + $okBus)
Write-Host ("General POI:    " + $okGeneral)
Write-Host ("MISS (kept old):" + $miss)
Write-Host ("Output: " + $outFile)
