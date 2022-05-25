@echo off
SETLOCAL

SET REV=%1

cd "%~dp0"
powershell -Command "(Get-Content winxpum\winxpum\winxpum.rc) -replace '\"ProductVersion\".*', '\"ProductVersion\", \"%REV%\"' | Out-File -encoding ASCII winxpum\winxpum\winxpum.rc"
"C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\Common7\IDE\devenv.com" winxpum\winxpum.sln /Rebuild "release|x64"
..\install\tools\signfile\SignFile.exe -u %SIGN_USER% -p %SIGN_PASS% -conf ..\install\tools\signfile\SignFileWin.config.xml winxpum\Release\xpumcli.exe