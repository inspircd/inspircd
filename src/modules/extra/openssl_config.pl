#!/usr/bin/perl

use lib "../..";
use make::utilities;

if ($ARGV[0] eq "compile")
{
	print pkgconfig_get_include_dirs("openssl", "/openssl/ssl.h", ""); 
}
else
{
	print pkgconfig_get_lib_dirs("openssl", "/libssl.so", "-lssl -lcrypto");
}

