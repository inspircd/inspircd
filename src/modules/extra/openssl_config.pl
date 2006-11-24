#!/usr/bin/perl

$ENV{PKG_CONFIG_PATH} = "/usr/lib/pkgconfig:/usr/local/lib/pkgconfig:/usr/local/libdata/pkgconfig:/usr/X11R6/libdata/pkgconfig";

if ($ARGV[1] eq "compile")
{
	$ret = `pkg-config --cflags openssl`;
	if ((undef $ret) || ($ret eq ""))
	{
		$ret = "";
	}
}
else
{
	$ret = `pkg-config --libs openssl`;
	if ((undef $ret) || ($ret eq ""))
	{
		$ret = "-lssl -lcrypto -ldl";
	}
}
print "$ret\n";
