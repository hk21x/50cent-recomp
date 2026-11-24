@echo off
setlocal
rem The release's package, built on a PC with the game (a build needs its xbe): Bulletproof-package.zip and its description, for
rem the release beside BulletproofLauncher.exe. Holds the runtime and the compiler, and no game code. Run build.bat first.
cd /d "%~dp0"
set "STAGE=out\package"
if exist "%STAGE%" rd /s /q "%STAGE%"
call "%~dp0prebuilt-test.bat" "%STAGE%" || exit /b 1
rd /s /q "%STAGE%\assets" 2>nul
del /q Bulletproof-package.zip Bulletproof-package.zip.json 2>nul
tar -a -c -f Bulletproof-package.zip -C "%STAGE%" . || (echo Making the zip failed. & exit /b 1)
powershell -NoProfile -Command "$f = Get-Item Bulletproof-package.zip; $h = (Get-FileHash $f -Algorithm SHA256).Hash.ToLower(); '{\"size\": ' + $f.Length + ', \"sha256\": \"' + $h + '\"}' | Set-Content -Encoding ascii Bulletproof-package.zip.json" || exit /b 1
for %%f in (Bulletproof-package.zip) do echo Bulletproof-package.zip: %%~zf bytes
echo To test: put it beside a launcher built by launcher.bat, in an empty folder.
echo To release: attach Bulletproof-package.zip and Bulletproof-package.zip.json to the tag's release.
endlocal
