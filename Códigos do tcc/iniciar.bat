@echo off
title Dashboard Robo 4x4 Cortador
echo Iniciando servidor local...
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0server.ps1"
pause
