@echo off
setlocal
cd /d "%~dp0"
set "ATS_DIAG_HOME=%~dp0"
echo ATS - diagnostic independant des mods.
echo Mode approfondi : GPU NVIDIA, memoire video, threads et piles CPU.
echo Une demande administrateur Windows va s'ouvrir pour le tracage ETW.
echo Lance ATS, puis conduis pendant trois minutes.
echo Ctrl+Maj+F8 : marquer une saccade. Ctrl+C dans le diagnostic : arreter.
echo Aucun mod, profil ou fichier du jeu n'est modifie.
echo.
powershell.exe -NoProfile -Command "$exe=Join-Path $env:ATS_DIAG_HOME 'ATSModDiagnostics.exe'; $docs=Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'American Truck Simulator'; $arguments=@('capture','--seconds','180','--documents',([string][char]34+$docs+[char]34)); try { $p=Start-Process -FilePath $exe -ArgumentList $arguments -Verb RunAs -PassThru; $p.WaitForExit(); $p.Refresh(); if($p.ExitCode -ne 0){Write-Host 'La capture a echoue ou a ete refusee.'; exit 1}; $root=Join-Path $env:ATS_DIAG_HOME 'captures'; $report=Get-ChildItem -LiteralPath $root -Filter 'rapport.html' -File -Recurse | Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1; if($report){Start-Process -FilePath $report.FullName} } catch { Write-Error $_; exit 1 }"
echo.
echo Les resultats sont dans le dossier captures, a cote de cet outil.
pause
