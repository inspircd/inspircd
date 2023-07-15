101		ICON		"inspircd.ico"

1 VERSIONINFO
  FILEVERSION    @VERSION_MAJOR@,@VERSION_MINOR@,@VERSION_PATCH@
  PRODUCTVERSION @VERSION_MAJOR@,@VERSION_MINOR@,@VERSION_PATCH@
  FILEFLAGSMASK  0x3fL
#ifdef _DEBUG
  FILEFLAGS 0x1L
#else
  FILEFLAGS 0x0L
#endif
  FILEOS 0x40004L
  FILETYPE 0x1L
  FILESUBTYPE 0x0L
BEGIN
    BLOCK "StringFileInfo"
    BEGIN
        BLOCK "040904b0"
        BEGIN
            VALUE "Comments", "InspIRCd @VERSION_MAJOR@.@VERSION_MINOR@ IRC Daemon"
            VALUE "CompanyName", "InspIRCd Development Team"
            VALUE "FileDescription", "InspIRCd"
            VALUE "FileVersion", "@FULL_VERSION@"
            VALUE "InternalName", "InspIRCd"
            VALUE "LegalCopyright", "Copyright (C) InspIRCd Development Team"
            VALUE "OriginalFilename", "inspircd.exe"
            VALUE "ProductName", "InspIRCd - Internet Relay Chat Daemon"
            VALUE "ProductVersion", "@FULL_VERSION@"
        END
    END
    BLOCK "VarFileInfo"
    BEGIN
        VALUE "Translation", 0x809, 1200
    END
END
