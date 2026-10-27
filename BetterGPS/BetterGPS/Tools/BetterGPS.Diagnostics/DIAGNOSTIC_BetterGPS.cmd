@echo off
cd /d "%~dp0"
echo BetterGPS - mesure des performances pendant 3 minutes.
echo Lance ATS, puis retourne dans le jeu pendant la capture.
echo Ctrl+Maj+F8 : marquer une saccade. Ctrl+C : terminer plus tot.
echo.
if exist "PresentMon.exe" (
  BetterGPSDiagnostics.exe capture --seconds 180 --presentmon "%~dp0PresentMon.exe"
) else (
  BetterGPSDiagnostics.exe capture --seconds 180
)
echo.
echo Les rapports HTML et JSON sont dans le dossier captures.
pause
