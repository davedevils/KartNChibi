@echo off
rem MSBuild is found through vswhere by run_msbuild.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0run_msbuild.ps1"
