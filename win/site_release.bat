@echo off

echo Release commencing...

cd ..

rem make binary
"c:\program files\nsis\makensis.exe" inspircd-noextras.nsi

rem determine name for the binary
perl rename_installer.pl

@echo on