@echo off
wmic process where (executablepath="%1") call terminate
pause