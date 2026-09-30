@echo off
call "%~dp0tools\GenerateProjectFiles_X64.bat" "Visual Studio 18 2026"
EXIT /B %ERRORLEVEL%
