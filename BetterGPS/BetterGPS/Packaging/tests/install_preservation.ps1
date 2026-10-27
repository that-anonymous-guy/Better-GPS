$ErrorActionPreference='Stop'
$workspace=(Resolve-Path (Join-Path $PSScriptRoot '../../..')).Path
$fixture=Join-Path $workspace ('build/diagnostics/installer-'+[guid]::NewGuid().ToString('N'))
$package=Join-Path $fixture 'package'
$game=Join-Path $fixture 'ATS'
$installed=Join-Path $game 'bin/win_x64/plugins/BetterGPS'
$source=Join-Path $package 'plugins/BetterGPS'
New-Item -ItemType Directory -Path $source,$installed,(Join-Path $source 'BetterGPS_assets/material/garmin') -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $installed 'BetterGPS_assets/material/garmin'),(Join-Path $installed 'device-settings') -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $workspace 'BetterGPS/Packaging/Install-BetterGPS.ps1') -Destination $package
function WriteFixture($path,$text){[IO.File]::WriteAllText($path,$text)}
WriteFixture (Join-Path $game 'bin/win_x64/amtrucks.exe') 'fixture only'
WriteFixture (Join-Path $package 'plugins/BetterGPS.dll') 'new loader'
WriteFixture (Join-Path $source 'BetterGPSOverlay.dll') 'new DLL'
WriteFixture (Join-Path $source 'BetterGPSDevices.ini') 'default device config'
WriteFixture (Join-Path $installed 'BetterGPSDevices.ini') 'custom devices and cameras'
WriteFixture (Join-Path $installed 'device-settings/device_10_horizontal_google.cfg') 'custom Crown placement'
$sourceAsset=Join-Path $source 'BetterGPS_assets/material/garmin/custom.dds'
$targetAsset=Join-Path $installed 'BetterGPS_assets/material/garmin/custom.dds'
WriteFixture $sourceAsset 'new default resource'
WriteFixture $targetAsset 'modified personal resource'
WriteFixture (Join-Path $source 'BetterGPS_assets/material/garmin/new.dds') 'new Garmin module'
$stockSource=Join-Path $source 'BetterGPS_assets/material/garmin/stock.dds'
$stockTarget=Join-Path $installed 'BetterGPS_assets/material/garmin/stock.dds'
WriteFixture $stockSource 'updated stock module'
WriteFixture $stockTarget 'old default module'
$oldHash=(Get-FileHash -LiteralPath $stockTarget).Hash
$receipt=@{files=@(@{path='plugins/BetterGPS/BetterGPS_assets/material/garmin/stock.dds';sha256=$oldHash},@{path='plugins/BetterGPS/BetterGPS_assets/material/garmin/custom.dds';sha256='different default hash'})}
WriteFixture (Join-Path $installed 'BetterGPSPackageManifest.json') ($receipt | ConvertTo-Json -Depth 5)
WriteFixture (Join-Path $package 'MANIFEST.json') (@{files=@(@{path='plugins/BetterGPS/BetterGPSOverlay.dll';sha256='fixture'})} | ConvertTo-Json -Depth 5)
$oldIcons=Join-Path $installed 'BetterGPS_assets/material/ui/phone_home/icons'
New-Item -ItemType Directory -Path $oldIcons -Force | Out-Null
WriteFixture (Join-Path $oldIcons 'android_home_normalmix.dds') 'retired edited icon'
WriteFixture (Join-Path $oldIcons 'my-custom_home_normalmix.dds') 'personal icon'
& (Join-Path $package 'Install-BetterGPS.ps1') -AtsPath $game
function Verify($path,$expected){if([IO.File]::ReadAllText($path) -ne $expected){throw ('Installer overwrote or failed to update '+$path)}}
Verify (Join-Path $installed 'BetterGPSDevices.ini') 'custom devices and cameras'
Verify (Join-Path $installed 'device-settings/device_10_horizontal_google.cfg') 'custom Crown placement'
Verify $targetAsset 'modified personal resource'
Verify $stockTarget 'updated stock module'
Verify (Join-Path $installed 'BetterGPS_assets/material/garmin/new.dds') 'new Garmin module'
Verify (Join-Path $installed 'BetterGPSOverlay.dll') 'new DLL'
if(Test-Path -LiteralPath (Join-Path $oldIcons 'android_home_normalmix.dds')){throw 'Retired Android default remains active'}
Verify (Join-Path $oldIcons 'my-custom_home_normalmix.dds') 'personal icon'
$retired=@(Get-ChildItem -LiteralPath (Join-Path $installed 'backups') -Filter android_home_normalmix.dds -Recurse -File)
if($retired.Count -ne 1){throw 'Retired icon was not backed up exactly once'}
Verify $retired[0].FullName 'retired edited icon'
$legacyGame=Join-Path $fixture 'ATS-legacy'
$legacyPlugin=Join-Path $legacyGame 'bin/win_x64/plugins/BetterGPS'
$legacyAssets=Join-Path $legacyPlugin 'OldOverlay_assets'
New-Item -ItemType Directory -Path (Join-Path $legacyAssets 'material/garmin'),(Join-Path $legacyAssets 'native_themes') -Force | Out-Null
WriteFixture (Join-Path $legacyGame 'bin/win_x64/amtrucks.exe') 'fixture only'
WriteFixture (Join-Path $legacyAssets 'material/garmin/custom.dds') 'legacy customized resource'
& (Join-Path $package 'Install-BetterGPS.ps1') -AtsPath $legacyGame
Verify (Join-Path $legacyPlugin 'BetterGPS_assets/material/garmin/custom.dds') 'legacy customized resource'
Write-Output 'PASS: package installer preserves device configuration, Crown placements and customized assets while updating unmodified defaults, adding Garmin resources and replacing the DLL.'
