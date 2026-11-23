@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\record-gameplay.ps1" -Action Videos
if errorlevel 1 pause
