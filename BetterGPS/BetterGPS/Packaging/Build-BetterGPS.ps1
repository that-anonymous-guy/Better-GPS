param([ValidateSet('Debug','Release')][string]$Configuration='Release', [ValidateRange(1,32)][int]$Jobs=4)
$ErrorActionPreference='Stop'
$root=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$build=Join-Path $root 'build'
cmake -S $root -B $build -A x64
if($LASTEXITCODE -ne 0){exit $LASTEXITCODE}
cmake --build $build --config $Configuration --parallel $Jobs
if($LASTEXITCODE -ne 0){exit $LASTEXITCODE}
dotnet publish (Join-Path $root 'BetterGPS/MapBridge/BetterGPSMapBridge.csproj') -c $Configuration -o (Join-Path $root 'BetterGPS/MapBridge/publish')
if($LASTEXITCODE -ne 0){exit $LASTEXITCODE}
dotnet publish (Join-Path $root 'BetterGPS/Tools/BetterGPS.Diagnostics/BetterGPS.Diagnostics.csproj') -c $Configuration -o (Join-Path $root 'BetterGPS/Tools/BetterGPS.Diagnostics/publish')
if($LASTEXITCODE -ne 0){exit $LASTEXITCODE}
dotnet publish (Join-Path $root 'BetterGPS/Overlay/tools/AssetInspector/AssetInspector.csproj') -c $Configuration -o (Join-Path $root 'BetterGPS/Overlay/tools/AssetInspector/publish')
exit $LASTEXITCODE
