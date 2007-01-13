package make::utilities;
use Exporter 'import';
@EXPORT = qw(make_rpath pkgconfig_get_include_dirs pkgconfig_get_lib_dirs);

# Parse the output of a *_config program,
# such as pcre_config, take out the -L
# directive and return an rpath for it.

sub make_rpath($)
{
	my ($executable) = @_;
	$data = `$executable`;
	$data =~ /-L(\S+)\s/;
	$libpath = $1;
	return "-Wl,--rpath -Wl,$libpath";
}

sub extend_pkg_path()
{
	if (!exists $ENV{PKG_CONFIG_PATH})
	{
		$ENV{PKG_CONFIG_PATH} = "/usr/lib/pkgconfig:/usr/local/lib/pkgconfig:/usr/local/libdata/pkgconfig:/usr/X11R6/libdata/pkgconfig";
	}
	else
	{
		$ENV{PKG_CONFIG_PATH} .= ":/usr/local/lib/pkgconfig:/usr/local/libdata/pkgconfig:/usr/X11R6/libdata/pkgconfig";
	}
}

sub pkgconfig_get_include_dirs($$$)
{
	my ($packagename, $headername, $defaults) = @_;
	extend_pkg_path();

	$ret = `pkg-config --cflags $packagename 2>/dev/null`;
	if ((!defined $ret) || ($ret eq ""))
	{
		$foo = `locate "$headername" | head -n 1`;
		$foo =~ /(.+)\Q$headername\E/;
		if (defined $1)
		{
			$foo = "-I$1";
		}
		else
		{
			$foo = "";
		}
		$ret = "$foo";
	}
	if (($defaults ne "") && (($ret eq "") || (!defined $ret)))
	{
		$ret = "$foo " . $defaults;
	}
	return $ret;
}

sub pkgconfig_get_lib_dirs($$$)
{
	my ($packagename, $libname, $defaults) = @_;
	extend_pkg_path();

	$ret = `pkg-config --libs $packagename 2>/dev/null`;
	if ((!defined $ret) || ($ret eq ""))
	{
		$foo = `locate "$libname" | head -n 1`;
		$foo =~ /(.+)\Q$libname\E/;
		if (defined $1)
		{
			$foo = "-L$1";
		}
		else
		{
			$foo = "";
		}
		$ret = "$foo";
	}

	if (($defaults ne "") && (($ret eq "") || (!defined $ret)))
	{
		$ret = "$foo " . $defaults;
	}
	return $ret;
}

1;

