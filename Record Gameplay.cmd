@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\record-gameplay.ps1" -Action Start
if errorlevel 1 pause
