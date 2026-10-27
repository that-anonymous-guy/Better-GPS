param([switch]$SkipBuild, [ValidateRange(1,32)][int]$Jobs=4, [string]$Python='python')
$ErrorActionPreference='Stop'
if(-not $SkipBuild){& (Join-Path $PSScriptRoot 'Build-BetterGPS.ps1') -Jobs $Jobs;if($LASTEXITCODE -ne 0){exit $LASTEXITCODE}}
& $Python (Join-Path $PSScriptRoot 'package_bettergps.py')
exit $LASTEXITCODE
