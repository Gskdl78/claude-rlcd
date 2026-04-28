@echo off
setlocal
set HERE=%~dp0
node "%HERE%..\src\hooks-emitter.js" %1
endlocal
