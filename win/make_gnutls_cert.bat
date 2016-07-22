@echo off

echo This program will generate SSL certificates for m_ssl_gnutls.so
echo Ensure certtool.exe is in your system path. It can be downloaded
echo at ftp://ftp.gnu.org/gnu/gnutls/w32/. If you do not know the answer
echo to one of the questions just press enter.
echo.

pause

certtool --generate-privkey --outfile conf/key.pem
certtool --generate-self-signed --load-privkey conf/key.pem --outfile conf/cert.pem

pause