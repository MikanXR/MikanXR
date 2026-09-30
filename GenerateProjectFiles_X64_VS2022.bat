@echo off
call "%~dp0tools\GenerateProjectFiles_X64.bat" "Visual Studio 17 2022"
EXIT /B %ERRORLEVEL%
