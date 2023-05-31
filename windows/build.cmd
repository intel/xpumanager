@echo on
SETLOCAL

SET REV=%1

SET WORK_DIR=%~dp0
cd %WORK_DIR%
for /f "tokens=3" %%I in ('type "winxpum\cli\resource.h" ^| findstr /c:"define VER_VERSION_MAJOR "') do set "VER_VERSION_MAJOR=%%I"
for /f "tokens=3" %%I in ('type "winxpum\cli\resource.h" ^| findstr /c:"define VER_VERSION_MINOR "') do set "VER_VERSION_MINOR=%%I"
for /f "tokens=3" %%I in ('type "winxpum\cli\resource.h" ^| findstr /c:"define VER_VERSION_PATCH "') do set "VER_VERSION_PATCH=%%I"
SET "version=%VER_VERSION_MAJOR%.%VER_VERSION_MINOR%.%VER_VERSION_PATCH%"
powershell -Command "(Get-Content winxpum\cli\resource.h) -replace 'VER_COMMIT_VERSION VER_VERSION_MAJORMINORPATCH_STR', 'VER_COMMIT_VERSION_TEMP \"%REV%\"' | Out-File  -encoding ASCII winxpum\cli\resource.h"
powershell -Command "(Get-Content winxpum\core\resource.h) -replace 'VER_COMMIT_VERSION VER_VERSION_MAJORMINORPATCH_STR', 'VER_COMMIT_VERSION_TEMP \"%REV%\"' | Out-File  -encoding ASCII winxpum\core\resource.h"
"C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\Common7\IDE\devenv.com" winxpum\winxpum.sln /Rebuild "release|x64"
pushd ..\install\tools\signfile
SignFile.exe -u %SIGN_USER% -p %SIGN_PASS% -conf SignFileWin.config.xml %WORK_DIR%\winxpum\Release\xpu-smi.exe
SignFile.exe -u %SIGN_USER% -p %SIGN_PASS% -conf SignFileWin.config.xml %WORK_DIR%\winxpum\Release\xpum.dll
popd
cd %WORK_DIR%\winxpum\Release
powershell Compress-Archive xpu-smi.exe,xpum.dll,xpum.lib,xpum_api.h,xpum_structs.h xpu-smi-%version%_win.zip
pushd ..\..\..\install\tools\signfile
SignFile.exe -u %SIGN_USER% -p %SIGN_PASS% -conf SignFileWin.config.xml -s cl -cf %WORK_DIR%\winxpum\Release\xpu-smi-%version%_win.zip.sig %WORK_DIR%\winxpum\Release\xpu-smi-%version%_win.zip
popd