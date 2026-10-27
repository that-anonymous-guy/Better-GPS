param([string]$AtsPath)
$ErrorActionPreference='Stop'
if(-not $AtsPath){$AtsPath=Read-Host 'Chemin du dossier American Truck Simulator'}
$game=(Resolve-Path -LiteralPath $AtsPath).Path
$plugins=Join-Path $game 'bin/win_x64/plugins'
if(-not (Test-Path -LiteralPath (Join-Path $game 'bin/win_x64/amtrucks.exe'))){throw 'Dossier ATS invalide : bin/win_x64/amtrucks.exe est introuvable.'}
$source=Join-Path $PSScriptRoot 'plugins'
if(-not (Test-Path -LiteralPath $source)){throw 'Package BetterGPS incomplet : dossier plugins introuvable.'}
if(Get-Process -Name amtrucks -ErrorAction SilentlyContinue){throw 'Ferme complètement ATS avant de mettre à jour BetterGPS.'}
New-Item -ItemType Directory -Path $plugins -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $source 'BetterGPS.dll') -Destination $plugins -Force
$component=Join-Path $source 'BetterGPS'
$destination=Join-Path $plugins 'BetterGPS'
$assetDestination=Join-Path $destination 'BetterGPS_assets'
if(-not(Test-Path -LiteralPath $assetDestination) -and (Test-Path -LiteralPath $destination)){
 # Preserve resources when updating a build that used an older assets name.
 $legacy=@(Get-ChildItem -LiteralPath $destination -Directory | Where-Object {
  $_.Name -like '*_assets' -and -not($_.Attributes -band [IO.FileAttributes]::ReparsePoint) -and
  (Test-Path -LiteralPath (Join-Path $_.FullName 'material')) -and
  (Test-Path -LiteralPath (Join-Path $_.FullName 'native_themes'))
 })
 if($legacy.Count -eq 1){Copy-Item -LiteralPath $legacy[0].FullName -Destination $assetDestination -Recurse}
}
$previous=@{}
$receipt=Join-Path $destination 'BetterGPSPackageManifest.json'
if(Test-Path -LiteralPath $receipt){
 foreach($file in (Get-Content -LiteralPath $receipt -Raw | ConvertFrom-Json).files){
  if($file.path.StartsWith('plugins/BetterGPS/')){$previous[$file.path.Substring(18)]=$file.sha256}
 }
}
$preserved=0
$retired=@('ui/phone_lockscreen/icons/android_normalmix.dds','ui/phone_lockscreen/icons/android_overlaymix.dds','ui/phone_lockscreen/icons/android_lock_normalmix.dds','ui/phone_lockscreen/icons/android_lock_overlaymix.dds','ui/phone_home/icons/android_home_normalmix.dds','ui/phone_home/icons/android_home_overlaymix.dds')
$retiredBackup=Join-Path $destination ('backups/retired-phone-assets-'+[guid]::NewGuid().ToString('N'))
foreach($relative in $retired){
 $target=Join-Path $assetDestination ('material/'+$relative)
 if(-not(Test-Path -LiteralPath $target -PathType Leaf)){continue}
 # Exact retired defaults only, with recoverable preservation even if edited.
 # Arbitrary user images and all configuration files are left alone.
 $backup=Join-Path $retiredBackup $relative
 New-Item -ItemType Directory -Path (Split-Path -Parent $backup) -Force | Out-Null
 Move-Item -LiteralPath $target -Destination $backup
}
foreach($file in Get-ChildItem -LiteralPath $component -File -Recurse){
 $relative=$file.FullName.Substring($component.Length+1)
 $key=$relative.Replace('\','/')
 $target=Join-Path $destination $relative
 $config=($relative -notmatch '[/\\]' -and $file.Extension -in '.cfg','.ini') -or $key.StartsWith('display-settings/') -or $key.StartsWith('device-settings/')
 $editable=$key.StartsWith('BetterGPS_assets/') -or $key.StartsWith('layouts/')
 if(Test-Path -LiteralPath $target){
  # Configurations always belong to the user. Update an editable resource only
  # when its bytes still match the default supplied by the previous package.
  if($config -or ($editable -and (-not $previous.ContainsKey($key) -or (Get-FileHash -LiteralPath $target).Hash -ne $previous[$key]))){$preserved++;continue}
 }
 New-Item -ItemType Directory -Path (Split-Path $target -Parent) -Force | Out-Null
 Copy-Item -LiteralPath $file.FullName -Destination $target -Force
}
$manifest=Join-Path $PSScriptRoot 'MANIFEST.json'
if(Test-Path -LiteralPath $manifest){Copy-Item -LiteralPath $manifest -Destination $receipt -Force}
Write-Host "BetterGPS installé dans $plugins ; $preserved fichiers personnalisables conservés."
