# SPDX-License-Identifier: GPL-3.0-or-later
$ErrorActionPreference = 'Stop'
$modelDirectory = Join-Path $PSScriptRoot 'model'
$modelPath = Join-Path $modelDirectory 'sentence-model.safetensors'
$expectedHash = '86ac529510cb3b4968a5e6ade83ec8080f5b34a0a681e75c362accbbd387d1c1'
New-Item -ItemType Directory -Path $modelDirectory -Force | Out-Null
if (!(Test-Path -LiteralPath $modelPath)) {
    $downloadPath = "$modelPath.part"
    Invoke-WebRequest -Uri 'https://huggingface.co/metasequoiaime/pinyin-ime-reranker-4M/resolve/main/sentence-model.safetensors' -OutFile $downloadPath
    if ((Get-FileHash -LiteralPath $downloadPath -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expectedHash) {
        throw "Model checksum mismatch: $downloadPath"
    }
    Move-Item -LiteralPath $downloadPath -Destination $modelPath
}
if ((Get-FileHash -LiteralPath $modelPath -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expectedHash) {
    throw "Model checksum mismatch: $modelPath"
}
Write-Output "Verified model: $modelPath"
