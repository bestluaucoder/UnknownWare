@echo off
echo ========================================
echo    RBX External - Offset Updater
echo ========================================
echo.

REM Check if Python is installed
python --version >nul 2>&1
if errorlevel 1 (
    echo [!] Python not found. Please install Python 3 first.
    echo [!] Download from: https://www.python.org/downloads/
    pause
    exit /b 1
)

REM Check if requests module is installed
python -c "import requests" >nul 2>&1
if errorlevel 1 (
    echo [*] Installing required Python package: requests
    pip install requests
    echo.
)

REM Run the updater
python update_offsets.py

echo.
pause
