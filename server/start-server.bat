@echo off
REM RO-MP Dedicated Server - start script (Windows)
cd /d "%~dp0"
bin\windows\romp-server.exe -c server.cfg
pause
