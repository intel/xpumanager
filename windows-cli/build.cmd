@echo on
SETLOCAL

SET REV=%1

SET WORK_DIR=%~dp0
cd %WORK_DIR%
powershell -Command "(Get-Content winxpum\winxpum\resource.h) -replace 'VER_COMMIT_VERSION VER_VERSION_MAJORMINORPATCH_STR', 'VER_COMMIT_VERSION_TEMP \"%REV%\"' | Out-File  -encoding ASCII winxpum\winxpum\resource.h"
"C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\Common7\IDE\devenv.com" winxpum\winxpum.sln /Rebuild "release|x64"
pushd ..\install\tools\signfile
SignFile.exe -u %SIGN_USER% -p %SIGN_PASS% -conf SignFileWin.config.xml %WORK_DIR%\winxpum\Release\xpumcli.exe
popd