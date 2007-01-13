#!/usr/bin/perl
if (!exists $ENV{PKG_CONFIG_PATH}) {
	$ENV{PKG_CONFIG_PATH} = "/usr/lib/pkgconfig:/usr/local/lib/pkgconfig:/usr/local/libdata/pkgconfig:/usr/X11R6/libdata/pkgconfig";
}
else {
	$ENV{PKG_CONFIG_PATH} .= ":/usr/local/lib/pkgconfig:/usr/local/libdata/pkgconfig:/usr/X11R6/libdata/pkgconfig";
}
if ($ARGV[0] eq "compile") {
	$ret = `pkg-config --cflags sqlite 2>/dev/null`; if ((!defined $ret) || ($ret eq "")) {
		$foo = `locate "/sqlite3.h" | head -n 1`; $foo =~ /(.+)\/sqlite3\.h/; if (defined $1) {
			$foo = "-I$1";
		}
		else {
			$foo = "";
		}
		$ret = "$foo\n";
	}
}
else {
	$ret = `pkg-config --libs sqlite3 2>/dev/null`; if ((!defined $ret) || ($ret eq "")) {
		$foo = `locate "/libsqlite3.so" | head -n 1`; $foo =~ /(.+)\/libsqlite3\.so/; if (defined $1) {
			$foo = "-L$1";
		}
		else {
			$foo = "";
		}
		$ret = "$foo -lsqlite3\n";
	}
}
print "$ret";
