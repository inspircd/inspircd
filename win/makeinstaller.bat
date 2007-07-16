@mkdir d:\temp\
move ..\bin\release\modules\m_ssl_gnutls.so d:\temp\
move ..\bin\release\modules\m_sslinfo.so d:\temp\
move ..\bin\release\modules\m_ssl_oper_cert.so d:\temp\
move ..\bin\release\modules\m_filter_pcre.so d:\temp
"C:\Program Files\NSIS\makensisw.exe" "inspircd.nsi"
move d:\temp\*.so ..\bin\release\modules\
