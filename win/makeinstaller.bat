move ..\bin\release\modules\m_ssl_gnutls.so c:\temp\
move ..\bin\release\modules\m_sslinfo.so c:\temp\
move ..\bin\release\modules\m_ssl_oper_cert.so c:\temp\
move ..\bin\release\modules\m_filter_pcre.so c:\temp\
"C:\Program Files\NSIS\makensisw.exe" "inspircd.nsi"
move c:\temp\m_ssl_gnutls.so ..\bin\release\modules
move c:\temp\m_sslinfo.so ..\bin\release\modules
move c:\temp\m_ssl_oper_cert.so ..\bin\release\modules
move c:\temp\m_filter_pcre.so ..\bin\release\modules

