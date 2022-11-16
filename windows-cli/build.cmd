@echo on
SETLOCAL

SET REV=%1

SET WORK_DIR=%~dp0
cd %WORK_DIR%
powershell -Command "(Get-Content winxpum\winxpum\winxpum.rc) -replace '\"ProductVersion\".*', '\"ProductVersion\", \"%REV%\"' | Out-File -encoding ASCII winxpum\winxpum\winxpum.rc"
"C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\Common7\IDE\devenv.com" winxpum\winxpum.sln /Rebuild "release|x64"
pushd ..\install\tools\signfile
SignFile.exe -u %SIGN_USER% -p %SIGN_PASS% -conf SignFileWin.config.xml %WORK_DIR%\winxpum\Release\xpumcli.exe
popd