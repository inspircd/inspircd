#!/usr/bin/perl

use lib "../..";
use make::utilities;

if ($ARGV[0] eq "compile")
{
        print pkgconfig_get_include_dirs("sqlite", "/sqlite3.h", "");
}
else    
{       
        print pkgconfig_get_lib_dirs("sqlite", "/libsqlite3.so", "-lsqlite3");
}
