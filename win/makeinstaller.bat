move ..\bin\release\modules\m_ssl_gnutls.so %TEMP%
move ..\bin\release\modules\m_sslinfo.so %TEMP%
move ..\bin\release\modules\m_ssl_oper_cert.so %TEMP%
move ..\bin\release\modules\m_filter_pcre.so %TEMP%
"C:\Program Files\NSIS\makensisw.exe" "inspircd.nsi"
move %TEMP%\m_ssl_gnutls.so ..\bin\release\modules
move %TEMP%\m_sslinfo.so ..\bin\release\modules
move %TEMP%\m_ssl_oper_cert.so ..\bin\release\modules
move %TEMP%\m_filter_pcre.so ..\bin\release\modules

