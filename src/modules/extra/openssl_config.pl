#!/usr/bin/perl

if (!exists $ENV{PKG_CONFIG_PATH})
{
	$ENV{PKG_CONFIG_PATH} = "/usr/lib/pkgconfig:/usr/local/lib/pkgconfig:/usr/local/libdata/pkgconfig:/usr/X11R6/libdata/pkgconfig";
}
else
{
	$ENV{PKG_CONFIG_PATH} .= ":/usr/local/lib/pkgconfig:/usr/local/libdata/pkgconfig:/usr/X11R6/libdata/pkgconfig";
}

if ($ARGV[0] eq "compile")
{
	$ret = `pkg-config --cflags openssl 2>/dev/null`;
	if ((!defined $ret) || ($ret eq ""))
	{
		$foo = `locate "openssl/ssl.h"`;
		$foo =~ s/\/openssl\/ssl\.h//;
		$ret = "-I$foo";
	}
}
else
{
	$ret = `pkg-config --libs openssl 2>/dev/null`;
	if ((!defined $ret) || ($ret eq ""))
	{
		$foo = `locate "/libssl.so" | head -n 1`;
		$foo =~ /(.+)\/libssl\.so/;
		if (defined $1)
		{
			$foo = "-L$1";
		}
		else
		{
			$foo = "";
		}
		$ret = "$foo -lssl -lcrypto\n";
	}
}
print "$ret";
