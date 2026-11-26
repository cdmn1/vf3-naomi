param(
    [Parameter(Mandatory=$true)][string]$AssetsPath,
    [string]$OutputPath = ""
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$manifest = Join-Path $root "GDROM_DISC_FILE_MAP.csv"
$map = Join-Path $root "vf3-netdimm-306mb-map.json"
$header = Join-Path $root "vf3_asset_map.h"
$baseRom = Get-ChildItem -Path $root -Filter "vf3-naomi-assetmap-base-v*.bin" | Select-Object -First 1
if (-not $baseRom) { throw "Map-aware base ROM was not found beside this script." }
if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $version = [regex]::Match($baseRom.Name, "base-v(\d+)\.bin").Groups[1].Value
    if (-not $version) { throw "Could not determine build version from base ROM filename." }
    $OutputPath = Join-Path (Get-Location) "vf3-naomi-chainload-v$version.bin"
}
if (-not (Test-Path $AssetsPath)) { throw "Assets folder not found: $AssetsPath" }
$packer = Join-Path $root "tools/pack_vf3_assets.py"
$append = Join-Path $root "tools/append_vf3_assets_to_naomi_rom.py"
$payload = Join-Path $env:TEMP "vf3-netdimm-306mb.payload"
Write-Host "Packing 411 manifest assets; MOVIE_A.SFD is excluded..."
python $packer --assets $AssetsPath --manifest $manifest --output $payload --map $map --exclude MOVIE_A.SFD
if ($LASTEXITCODE -ne 0) { throw "Asset packing failed (exit $LASTEXITCODE)." }
$headerText = Get-Content $header -Raw
$m = [regex]::Match($headerText, '(?m)^#define VF3_ASSET_PAYLOAD_BASE (\d+)u$')
if (-not $m.Success) { throw "Could not read payload base from vf3_asset_map.h." }
$payloadBase = [int64]$m.Groups[1].Value
Write-Host "Combining base ROM and packed payload at offset $payloadBase..."
python $append --rom $baseRom.FullName --payload $payload --map-json $map --header $header --payload-base $payloadBase --output $OutputPath
if ($LASTEXITCODE -ne 0) { throw "Image assembly failed (exit $LASTEXITCODE)." }
Remove-Item $payload -Force
$size = (Get-Item $OutputPath).Length
$sha = (Get-FileHash $OutputPath -Algorithm SHA256).Hash.ToLower()
Write-Host "Created: $OutputPath"
Write-Host ("Size: {0:N0} bytes ({1:N2} MiB)" -f $size,($size/1MB))
Write-Host "SHA-256: $sha"
Write-Host "Ready for Flycast: expected RETAIL 1ST_READ LOAD: PASS and 1ST_READ SHA256: MATCH, followed by a chainload attempt after 3 seconds."
