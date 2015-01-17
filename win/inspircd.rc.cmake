101		ICON		"inspircd.ico"

1 VERSIONINFO
  FILEVERSION    @MAJOR_VERSION@,@MINOR_VERSION@,@PATCH_VERSION@
  PRODUCTVERSION @MAJOR_VERSION@,@MINOR_VERSION@,@PATCH_VERSION@
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
            VALUE "Comments", "InspIRCd @MAJOR_VERSION@.@MINOR_VERSION@ IRC Daemon"
            VALUE "CompanyName", "InspIRCd Development Team"
            VALUE "FileDescription", "InspIRCd"
            VALUE "FileVersion", "@FULL_VERSION@"
            VALUE "InternalName", "InspIRCd"
            VALUE "LegalCopyright", "Copyright (c) 2015 InspIRCd Development Team"
            VALUE "OriginalFilename", "inspircd.exe"
            VALUE "ProductName", "InspIRCd - The Inspire IRC Daemon"
            VALUE "ProductVersion", "@FULL_VERSION@"
        END
    END
    BLOCK "VarFileInfo"
    BEGIN
        VALUE "Translation", 0x809, 1200
    END
END
