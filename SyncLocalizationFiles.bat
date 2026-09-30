@echo off
setlocal

:: The repo's .venv carries polib (tools\SetupDevEnvironment.bat creates it)
set "PYTHON=python"
if exist "%~dp0.venv\Scripts\python.exe" set "PYTHON=%~dp0.venv\Scripts\python.exe"

echo "Sync new strings from English source"
"%PYTHON%" "%~dp0tools\localization.py" sync

echo "Verify string data"
"%PYTHON%" "%~dp0tools\localization.py" check
