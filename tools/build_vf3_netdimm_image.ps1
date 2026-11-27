param(
    [string]$AssetsPath = "",
    [string]$OutputPath = "",
    [string]$AssetsZipUrl = "https://www.dropbox.com/scl/fo/l47sloq9477lxnotdix35/AFbQ2trl_xczZdgC9fHZgZg?rlkey=uriqyifyb3igd74a31iijz92h&dl=1"
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$manifest = Join-Path $root "GDROM_DISC_FILE_MAP.csv"
$map = Join-Path $root "vf3-netdimm-306mb-map.json"
$header = Join-Path $root "vf3_asset_map.h"
$baseRom = Get-ChildItem -Path $root -Filter "vf3-naomi-assetmap-base-v*.bin" | Select-Object -First 1
$packer = Join-Path $root "tools/pack_vf3_assets.py"
$append = Join-Path $root "tools/append_vf3_assets_to_naomi_rom.py"

foreach ($required in @($manifest, $map, $header, $packer, $append)) {
    if (-not (Test-Path $required)) { throw "Required build file is missing: $required" }
}
if (-not $baseRom) { throw "The compact v81 base ROM is missing. Extract the vf3-naomi-compact-map-base-v81 ZIP into this same folder." }

$work = Join-Path ([System.IO.Path]::GetTempPath()) ("vf3-naomi-build-" + [guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $work | Out-Null
try {
    if ([string]::IsNullOrWhiteSpace($AssetsPath)) {
        $zip = Join-Path $work "vf3-assets.zip"
        $extract = Join-Path $work "assets"
        Write-Host "Downloading the VF3 asset archive from the project's Dropbox source..."
        Invoke-WebRequest -Uri $AssetsZipUrl -OutFile $zip
        Write-Host "Extracting asset archive..."
        Expand-Archive -LiteralPath $zip -DestinationPath $extract -Force
        $AssetsPath = $extract
    }
    if (-not (Test-Path $AssetsPath)) { throw "Assets folder not found: $AssetsPath" }

    if ([string]::IsNullOrWhiteSpace($OutputPath)) {
        $version = [regex]::Match($baseRom.Name, "base-v(\d+)\.bin").Groups[1].Value
        if (-not $version) { throw "Could not determine build version from base ROM filename." }
        $OutputPath = Join-Path (Get-Location) "vf3-naomi-v$version-full.bin"
    }
    $OutputPath = [System.IO.Path]::GetFullPath($OutputPath)
    $payload = Join-Path $work "vf3-netdimm-306mb.payload"
    $generatedMap = Join-Path $work "vf3-netdimm-306mb-map.json"

    Write-Host "Packing the 411 retail manifest assets (MOVIE_A.SFD excluded)..."
    python $packer --assets $AssetsPath --manifest $manifest --output $payload --map $generatedMap --exclude MOVIE_A.SFD
    if ($LASTEXITCODE -ne 0) { throw "Asset packing failed (exit $LASTEXITCODE)." }

    $headerText = Get-Content $header -Raw
    $m = [regex]::Match($headerText, '(?m)^#define VF3_ASSET_PAYLOAD_BASE (\d+)u$')
    if (-not $m.Success) { throw "Could not read payload base from vf3_asset_map.h." }
    $payloadBase = [int64]$m.Groups[1].Value
    if ($payloadBase -ne 358400) { throw "This base ROM expects payload base 0x57800 (358400), but header says $payloadBase. Use the matching v81 compact base package." }

    Write-Host "Checking generated asset map against the v81 map..."
    $expectedMap = Get-Content $map -Raw | ConvertFrom-Json
    $actualMap = Get-Content $generatedMap -Raw | ConvertFrom-Json
    if ($actualMap.payload_bytes -ne $expectedMap.payload_bytes -or $actualMap.packed_file_count -ne $expectedMap.packed_file_count) {
        throw "Generated payload map does not match the v81 compact map. Check that the Dropbox source is the expected VF3tb USA asset set."
    }

    Write-Host "Combining the v81 base ROM and generated payload..."
    python $append --rom $baseRom.FullName --payload $payload --map-json $generatedMap --header $header --payload-base $payloadBase --output $OutputPath
    if ($LASTEXITCODE -ne 0) { throw "Image assembly failed (exit $LASTEXITCODE)." }

    $size = (Get-Item $OutputPath).Length
    $sha = (Get-FileHash $OutputPath -Algorithm SHA256).Hash.ToLower()
    Write-Host ""
    Write-Host "SUCCESS - complete NAOMI image created:"
    Write-Host $OutputPath
    Write-Host ("Size: {0:N0} bytes ({1:N2} MiB)" -f $size,($size/1MB))
    Write-Host "SHA-256: $sha"
    Write-Host "The temporary payload was generated automatically and will be cleaned up."
}
finally {
    Remove-Item $work -Recurse -Force -ErrorAction SilentlyContinue
}
