@echo off
rem Copies the small Houdini bridge into Houdini 22.0's Python startup folder,
rem so the World Builder can ask the open Houdini to make the river.
rem Open Houdini 22.0 once before running this (it creates its prefs folder).
"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -Command ^
  "$docs = [Environment]::GetFolderPath('MyDocuments');" ^
  "$dest = Join-Path $docs 'houdini22.0\python3.13libs';" ^
  "New-Item -ItemType Directory -Force $dest | Out-Null;" ^
  "Copy-Item -Force '%~dp0bridge\iwb_houdini_bridge.py', '%~dp0bridge\uiready.py' $dest;" ^
  "Write-Host ('Houdini bridge installed in ' + $dest + '. Restart Houdini, then click Build World in Unreal.')"
pause
