@echo on
setlocal

set "PYTHON=python"
if exist "%~dp0.venv\Scripts\python.exe" set "PYTHON=%~dp0.venv\Scripts\python.exe"

"%PYTHON%" "%~dp0tools\token_stats.py" --render && "%PYTHON%" "%~dp0tools\token_stats.py" --check
