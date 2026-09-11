@echo off
chcp 65001 >nul
powershell.exe -NoExit -ExecutionPolicy Bypass -File "%~dp0scan_object.ps1"