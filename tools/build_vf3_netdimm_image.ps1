param(
    [Parameter(Mandatory=$true)]
    [ValidateNotNullOrEmpty()]
    [string]$AssetsPath,

    [string]$OutputPath = ""
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$manifest = Join-Path $root "GDROM_DISC_FILE_MAP.csv"
$map = Join-Path $root "vf3-netdimm-306mb-map.json"
$header = Join-Path $root "vf3_asset_map.h"
$packer = Join-Path $root "tools/pack_vf3_assets.py"
$append = Join-Path $root "tools/append_vf3_assets_to_naomi_rom.py"

foreach ($required in @($manifest, $map, $header, $packer, $append)) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "Required build file is missing: $required"
    }
}

$baseRoms = @(Get-ChildItem -LiteralPath $root -Filter "vf3-naomi-assetmap-base-v*.bin" -File)
if ($baseRoms.Count -ne 1) {
    throw "Expected exactly one compact base ROM in $root; found $($baseRoms.Count). Extract one vf3-naomi-compact-map-base package and keep its files together."
}
$baseRom = $baseRoms[0]

$assetInput = Get-Item -LiteralPath $AssetsPath -ErrorAction Stop
$python = Get-Command python -ErrorAction SilentlyContinue
if (-not $python) {
    throw "Python 3 was not found. Install Python 3 and ensure the 'python' command is available in PowerShell."
}
$pythonVersion = & python --version 2>&1
if ($LASTEXITCODE -ne 0 -or ($pythonVersion -notmatch "Python 3\.")) {
    throw "Python 3 is required. Detected: $pythonVersion"
}

if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $versionMatch = [regex]::Match($baseRom.Name, "base-v(\d+)\.bin")
    if (-not $versionMatch.Success) {
        throw "Could not determine build version from base ROM filename: $($baseRom.Name)"
    }
    $OutputPath = Join-Path (Get-Location) ("vf3-naomi-chainload-v{0}.bin" -f $versionMatch.Groups[1].Value)
}
$OutputPath = [System.IO.Path]::GetFullPath($OutputPath)

$work = Join-Path ([System.IO.Path]::GetTempPath()) ("vf3-naomi-build-" + [guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $work | Out-Null

try {
    if ($assetInput.PSIsContainer) {
        $assetsFullPath = (Resolve-Path -LiteralPath $assetInput.FullName).Path
        Write-Host "Using already-extracted local assets: $assetsFullPath"
    }
    elseif ($assetInput.Extension -ieq ".7z") {
        $sevenZip = Get-Command 7z.exe -ErrorAction SilentlyContinue
        if (-not $sevenZip) {
            $programFilesX86 = [Environment]::GetEnvironmentVariable("ProgramFiles(x86)")
            $candidates = @(
                (Join-Path $env:ProgramFiles "7-Zip\7z.exe"),
                (Join-Path $programFilesX86 "7-Zip\7z.exe")
            )
            foreach ($candidate in $candidates) {
                if ($candidate -and (Test-Path -LiteralPath $candidate -PathType Leaf)) {
                    $sevenZipPath = $candidate
                    break
                }
            }
            if (-not $sevenZipPath) {
                throw "A local .7z archive was supplied, but 7-Zip was not found. Install 7-Zip or pass the path to an already-extracted assets folder."
            }
        }
        else {
            $sevenZipPath = $sevenZip.Source
        }

        $assetsFullPath = Join-Path $work "extracted-assets"
        New-Item -ItemType Directory -Path $assetsFullPath | Out-Null
        Write-Host "Extracting local 7z archive with 7-Zip..."
        & $sevenZipPath x -y "-o$assetsFullPath" $assetInput.FullName
        if ($LASTEXITCODE -ne 0) {
            throw "7-Zip extraction failed (exit $LASTEXITCODE)."
        }
    }
    else {
        throw "AssetsPath must be either an extracted assets folder or a local .7z archive: $AssetsPath"
    }

    $payload = Join-Path $work "vf3-netdimm-306mb.payload"
    $generatedMap = Join-Path $work "vf3-netdimm-306mb-map.json"

    Write-Host "Base ROM: $($baseRom.Name)"
    Write-Host "Packing retail assets and validating file sizes/SHA-1..."
    & python $packer --assets $assetsFullPath --manifest $manifest --output $payload --map $generatedMap --exclude MOVIE_A.SFD
    if ($LASTEXITCODE -ne 0) {
        throw "Asset packing or validation failed (exit $LASTEXITCODE)."
    }

    $headerText = Get-Content -LiteralPath $header -Raw
    $headerMatch = [regex]::Match($headerText, '(?m)^#define VF3_ASSET_PAYLOAD_BASE (\d+)u$')
    if (-not $headerMatch.Success) {
        throw "Could not read VF3_ASSET_PAYLOAD_BASE from vf3_asset_map.h."
    }
    $payloadBase = [int64]$headerMatch.Groups[1].Value
    $baseLength = [int64](Get-Item -LiteralPath $baseRom.FullName).Length
    $expectedPayloadBase = [int64]([Math]::Ceiling($baseLength / 2048.0) * 2048)
    if ($payloadBase -ne $expectedPayloadBase) {
        throw ("The map header payload base 0x{0:X} does not match the sector-aligned base ROM end 0x{1:X}. Keep the base ROM, map header and manifest from the same package." -f $payloadBase, $expectedPayloadBase)
    }
    Write-Host ("Verified payload base: 0x{0:X} ({0} bytes)" -f $payloadBase)

    Write-Host "Checking generated payload map against the packaged retail map..."
    $expectedMap = Get-Content -LiteralPath $map -Raw | ConvertFrom-Json
    $actualMap = Get-Content -LiteralPath $generatedMap -Raw | ConvertFrom-Json
    if ($actualMap.payload_bytes -ne $expectedMap.payload_bytes -or
        $actualMap.packed_file_count -ne $expectedMap.packed_file_count -or
        $actualMap.source_file_count -ne $expectedMap.source_file_count) {
        throw "Generated payload map does not match the packaged VF3tb USA map. Confirm that AssetsPath points to the correct complete retail extraction."
    }

    Write-Host "Combining the compact NAOMI base and locally generated asset payload..."
    & python $append --rom $baseRom.FullName --payload $payload --map-json $generatedMap --header $header --payload-base $payloadBase --output $OutputPath
    if ($LASTEXITCODE -ne 0) {
        throw "Image assembly failed (exit $LASTEXITCODE)."
    }

    $size = (Get-Item -LiteralPath $OutputPath).Length
    $sha = (Get-FileHash -LiteralPath $OutputPath -Algorithm SHA256).Hash.ToLowerInvariant()
    Write-Host ""
    Write-Host "SUCCESS - complete NAOMI image created:"
    Write-Host $OutputPath
    Write-Host ("Size: {0:N0} bytes ({1:N2} MiB)" -f $size, ($size / 1MB))
    Write-Host "SHA-256: $sha"
    Write-Host "Temporary extracted files, payload and generated map were removed automatically."
}
finally {
    Remove-Item -LiteralPath $work -Recurse -Force -ErrorAction SilentlyContinue
}
