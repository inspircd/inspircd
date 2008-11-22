@echo off

echo release/makereleasetrunk.sh %2 > remote.txt
start "Linux build" "c:\Program Files\PuTTY\putty.exe" -load "inspircd release" -m remote.txt

echo option batch on > upload.scp
echo option confirm off >> upload.scp
echo put -speed=4 -nopermissions -preservetime %1 /usr/home/inspircd/www/downloads/ >> upload.scp
echo exit >> upload.scp
start "File upload" "c:\program files\winscp\winscp.com" "inspircd release" /script=upload.scp

@echo on
