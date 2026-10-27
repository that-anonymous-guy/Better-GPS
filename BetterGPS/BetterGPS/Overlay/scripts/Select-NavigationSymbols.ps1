$ErrorActionPreference='Stop'
$workspace=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$pack=(Resolve-Path (Join-Path $PSScriptRoot '../BetterGPS_assets/material/ui/rounded_material_symbols')).Path
$source=(Resolve-Path (Join-Path $pack 'ALL_ROUNDED')).Path
if(-not $source.StartsWith($workspace+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){throw 'Asset cleanup escaped workspace.'}
$groups=@{
 navigation='turn_left turn_right turn_sharp_left turn_sharp_right turn_slight_left turn_slight_right u_turn_left u_turn_right fork_left fork_right merge ramp_left ramp_right straight roundabout_left roundabout_right route alt_route directions navigation signpost road unpaved_road pin_road multiple_stop trip_origin flag flag_check north south east west north_east north_west south_east south_west'
 places='local_gas_station rest_area local_parking parking_sign car_repair tire_repair handyman local_shipping warehouse factory local_police local_hospital medical_services restaurant local_dining local_cafe fastfood hotel local_convenience_store local_atm directions_boat directions_railway toll traffic traffic_jam speed_camera'
 status='speed timer schedule update sync sync_problem wifi wifi_off signal_cellular_alt signal_cellular_off battery_full battery_alert battery_charging_full gps_fixed gps_not_fixed gps_off location_on location_off location_disabled location_searching my_location near_me warning error info help check_circle cancel do_not_disturb_on pause play_arrow notifications notifications_off report crisis_alert'
 controls='mic mic_off volume_up volume_off volume_down search close arrow_back arrow_forward menu more_vert settings tune layers map zoom_in zoom_out zoom_in_map zoom_out_map fullscreen fullscreen_exit refresh replay restart_alt stop exit_to_app expand_more expand_less keyboard_arrow_left keyboard_arrow_right dark_mode light_mode language download share bookmark star favorite home history delete'
}
$files=@(Get-ChildItem -LiteralPath $source -Recurse -File -Filter '*.svg')
$rows=@()
foreach($role in $groups.Keys){
 $destination=Join-Path $pack $role;New-Item -ItemType Directory -Path $destination -Force | Out-Null
 foreach($name in $groups[$role].Split(' ',[StringSplitOptions]::RemoveEmptyEntries)){
  $matches=@($files | Where-Object BaseName -eq $name);if($matches.Count -eq 0){continue}
  if($matches.Count -ne 1){throw "Ambiguous SVG: $name"}
  $file=$matches[0];$target=Join-Path $destination $file.Name
  Copy-Item -LiteralPath $file.FullName -Destination $target -Force
  $hash=(Get-FileHash -LiteralPath $file.FullName).Hash
  if((Get-FileHash -LiteralPath $target).Hash -ne $hash){throw "SVG copy mismatch: $name"}
  $rows+=@{name=$name;role=$role;original=$file.FullName.Substring($source.Length+1);sha256=$hash}
 }
}
if($rows.Count -lt 100){throw 'Selection unexpectedly incomplete; source preserved.'}
@{originalSvgCount=$files.Count;retainedSvgCount=$rows.Count;removedSvgCount=$files.Count-$rows.Count;symbols=$rows} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $pack 'curated_manifest.json') -Encoding utf8
# Exact resolved pack path checked above; copied assets were verified before removal.
Remove-Item -LiteralPath $source -Recurse -Force
$metadata=Join-Path $pack 'material_symbols_metadata_latest.json'
if(Test-Path -LiteralPath $metadata){Remove-Item -LiteralPath $metadata -Force}
Write-Output "Kept $($rows.Count) navigation/phone SVGs; removed $($files.Count-$rows.Count) unrelated SVGs."
