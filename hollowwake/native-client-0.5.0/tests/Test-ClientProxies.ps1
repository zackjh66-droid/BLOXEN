param([string]$OutputDirectory,[string]$RuntimeLibrary)
$ErrorActionPreference='Stop';$source=Split-Path $PSScriptRoot -Parent
$prepared=Join-Path $OutputDirectory 'OwnClientProxies'
python (Join-Path $PSScriptRoot 'prepare_proxy_fixture.py') $prepared
if($LASTEXITCODE){throw 'Exact native dependency proxy generation failed'}
& (Join-Path $source 'private-package/Build-Proxies.ps1') -PreparedDirectory $prepared -RuntimeLibrary $RuntimeLibrary
python (Join-Path $PSScriptRoot 'check_proxy_exports.py') $prepared (Join-Path $OutputDirectory 'client-proxy-results.json')
if($LASTEXITCODE){throw 'Compiled native dependency proxy metadata failed'}
