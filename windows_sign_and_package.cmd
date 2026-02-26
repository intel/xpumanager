@echo off
SETLOCAL

SET COMMIT=%1
if "%~1"=="" (
	echo Error: Missing commit id.
	exit /b 1
)
@echo on
SET COMMIT_SHORT=%COMMIT:~0,8%
for /f %%i in ('powershell -NoProfile -Command "Get-Date -Format ''MM-dd-yyyy''"') do set "TIMESTAMP=%%i"
SET WORK_DIR=%~dp0
echo %WORK_DIR%
SET BIN_DIR=%WORK_DIR%\builddir\ial\cli
SET SIGNFILE_DIR=%WORK_DIR%\signfile
set "COMMIT=%~1"
if "%~1"=="" (
	echo Error: Missing commit id.
	exit /b 1
)
@echo on
set "COMMIT_SHORT=%COMMIT:~0,8%"
set "TIMESTAMP=%DATE:~4,2%-%DATE:~7,2%-%DATE:~10,4%"
set "WORK_DIR=%~dp0"
echo %WORK_DIR%
set "BIN_DIR=%WORK_DIR%\builddir\ial\cli"
set "SIGNFILE_DIR=%WORK_DIR%\signfile"
cd "%WORK_DIR%"
SET /p VERSION=<"%WORK_DIR%\VERSION"
pushd "%SIGNFILE_DIR%"
@echo off
SignFile.exe -u %SIGN_USER% -p %SIGN_PASS% -conf SignFileWin.config.xml "%BIN_DIR%\xpu-smi.exe"
if errorlevel 1 (
	echo Error: Failed to sign xpu-smi.exe.
	exit /b 1
)
SignFile.exe -u %SIGN_USER% -p %SIGN_PASS% -conf SignFileWin.config.xml "%BIN_DIR%\xpum-2.dll"
if errorlevel 1 (
	echo Error: Failed to sign xpum-2.dll.
	exit /b 1
)
@echo on
popd
cd "%BIN_DIR%"
powershell -NoProfile -NonInteractive -Command "Compress-Archive -Path 'xpu-smi.exe','xpum-2.dll','xpu-smi.exp','xpu-smi.lib' -DestinationPath 'xpu-smi-%VERSION%-%TIMESTAMP%.%COMMIT_SHORT%_win.zip'"
pushd "%SIGNFILE_DIR%"
@echo off
SignFile.exe -u %SIGN_USER% -p %SIGN_PASS% -conf SignFileWin.config.xml -s cl -cf "%BIN_DIR%\xpu-smi-%VERSION%-%TIMESTAMP%.%COMMIT_SHORT%_win.zip.sig" "%BIN_DIR%\xpu-smi-%VERSION%-%TIMESTAMP%.%COMMIT_SHORT%_win.zip"
if ERRORLEVEL 1 (
    echo Error: Failed to sign archive %BIN_DIR%\xpu-smi-%VERSION%-%TIMESTAMP%.%COMMIT_SHORT%_win.zip
    exit /b 1
)
popd
