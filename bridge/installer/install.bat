@echo off
setlocal
set HERE=%~dp0
node "%HERE%install.mjs" "%HERE%emit.cmd"
endlocal
