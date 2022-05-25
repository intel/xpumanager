@echo off
SETLOCAL

SET SIGN_USER=%1
SET SIGN_PASS=%2

cd "%~dp0"
"C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\Common7\IDE\devenv.com" winxpum\winxpum.sln /Rebuild "release|x64"
..\install\tools\signfile\SignFile.exe -u %SIGN_USER% -p %SIGN_PASS% -conf ..\install\tools\signfile\SignFileWin.config.xml winxpum\Release\xpumcli.exe