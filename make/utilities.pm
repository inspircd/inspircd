package make::utilities;
use Exporter 'import';
@EXPORT = qw(make_rpath pkgconfig_get_include_dirs pkgconfig_get_lib_dirs translate_functions);

# Parse the output of a *_config program,
# such as pcre_config, take out the -L
# directive and return an rpath for it.

# \033[1;32msrc/Makefile\033[0m

my %already_added = ();

sub make_rpath($)
{
	my ($executable) = @_;
	chomp($data = `$executable`);
	my $output = "";
	while ($data =~ /-L(\S+)/)
	{
		$libpath = $1;
		chomp($libpath);
		if (!exists $already_added{$libpath})
		{
			print "Adding extra library path \033[1;32m$libpath\033[0m ...\n";
			$already_added{$libpath} = 1;
		}
		$output .= "-Wl,--rpath -Wl,$libpath -L$libpath ";
		$data =~ s/-L(\S+)//;
	}
	return $output;
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

	print "Locating include directory for package \033[1;32m$packagename\033[0m ... ";

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
	chomp($ret);
	if (($ret eq " ") || (!defined $ret))
	{
		print "\033[1;32mUsing defaults\033[0m\n";
	}
	else
	{
		print "\033[1;32m$ret\033[0m\n";
	}
	return $ret;
}

sub pkgconfig_get_lib_dirs($$$)
{
	my ($packagename, $libname, $defaults) = @_;
	extend_pkg_path();

	print "Locating library directory for package \033[1;32m$packagename\033[0m ... ";

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
	chomp($ret);
	if (($ret eq " ") || (!defined $ret))
	{
		print "\033[1;32mUsing defaults\033[0m\n";
	}
	else
	{
		print "\033[1;32m$ret\033[0m\n";
	}
	return $ret;
}

# Translate a $CompileFlags etc line and parse out function calls
# to functions within these modules at configure time.
sub translate_functions($)
{
	my ($line) = @_;
	while ($line =~ /pkgconflibs\("(.+?)","(.+?)","(.+?)"\)/)
	{
		my $replace = pkgconfig_get_lib_dirs($1, $2, $3);
		$line =~ s/pkgconflibs\("(.+?)","(.+?)","(.+?)"\)/$replace/;
	}
	while ($line =~ /pkgconflibs\("(.+?)","(.+?)",""\)/)
	{
		my $replace = pkgconfig_get_lib_dirs($1, $2, "");
		$line =~ s/pkgconflibs\("(.+?)","(.+?)",""\)/$replace/;
	}
	while ($line =~ /pkgconfincludes\("(.+?)","(.+?)",""\)/)
	{
		my $replace = pkgconfig_get_include_dirs($1, $2, "");
		$line =~ s/pkgconfincludes\("(.+?)","(.+?)",""\)/$replace/;
	}
	while ($line =~ /pkgconfincludes\("(.+?)","(.+?)","(.+?)"\)/)
	{
		my $replace = pkgconfig_get_include_dirs($1, $2, $3);
		$line =~ s/pkgconfincludes\("(.+?)","(.+?)","(.+?)"\)/$replace/;
	}
	while ($line =~ /rpath\("(.+?)"\)/)
	{
		my $replace = make_rpath($1);
		$line =~ s/rpath\("(.+?)"\)/$replace/;
	}
	return $line;
}

1;

