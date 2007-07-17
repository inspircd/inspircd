#       +------------------------------------+
#       | Inspire Internet Relay Chat Daemon |
#       +------------------------------------+
#
#  InspIRCd: (C) 2002-2007 InspIRCd Development Team
# See: http://www.inspircd.org/wiki/index.php/Credits
#
# This program is free but copyrighted software; see
#      the file COPYING for details.
#
# ---------------------------------------------------


package make::utilities;
use Exporter 'import';
use POSIX;
use Getopt::Long;
@EXPORT = qw(make_rpath pkgconfig_get_include_dirs pkgconfig_get_lib_dirs pkgconfig_check_version translate_functions promptstring vcheck);

# Parse the output of a *_config program,
# such as pcre_config, take out the -L
# directive and return an rpath for it.

# \033[1;32msrc/Makefile\033[0m

my %already_added = ();

sub promptstring($$$$$)
{
	my ($prompt, $configitem, $default, $package, $commandlineswitch) = @_;
	my $var;
	if (!$main::interactive)
	{
		undef $opt_commandlineswitch;
		GetOptions ("$commandlineswitch=s" => \$opt_commandlineswitch);
		if (defined $opt_commandlineswitch)
		{
			print "\033[1;32m$opt_commandlineswitch\033[0m\n";
			$var = $opt_commandlineswitch;
		}
		else
		{
			die "Could not detect $package! Please specify the $prompt via the command line option \033[1;32m--$commandlineswitch=\"/path/to/file\"\033[0m";
		}
	}
	else
	{
		print "\nPlease enter the $prompt?\n";
		print "[\033[1;32m$default\033[0m] -> ";
		chomp($var = <STDIN>);
	}
	if ($var eq "")
	{
		$var = $default;
	}
	$main::config{$configitem} = $var;
}

sub make_rpath($;$)
{
	my ($executable, $module) = @_;
	chomp($data = `$executable`);
	my $output = "";
	while ($data =~ /-L(\S+)/)
	{
		$libpath = $1;
		if (!exists $already_added{$libpath})
		{
			print "Adding extra library path to \033[1;32m$module\033[0m ... \033[1;32m$libpath\033[0m\n";
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

sub pkgconfig_get_include_dirs($$$;$)
{
	my ($packagename, $headername, $defaults, $module) = @_;

	my $key = "default_includedir_$packagename";
	if (exists $main::config{$key})
	{
		print "Locating include directory for package \033[1;32m$packagename\033[0m for module \033[1;32m$module\033[0m... ";
		$ret = $main::config{$key};
		print "\033[1;32m$ret\033[0m (cached)\n";
		return $ret;
	}

	extend_pkg_path();

	print "Locating include directory for package \033[1;32m$packagename\033[0m for module \033[1;32m$module\033[0m... ";

	$v = `pkg-config --modversion $packagename 2>/dev/null`;
	$ret = `pkg-config --cflags $packagename 2>/dev/null`;

	if ((!defined $v) || ($v eq ""))
	{
		$foo = `locate "$headername" | head -n 1`;
		$foo =~ /(.+)\Q$headername\E/;
		$find = $1;
		chomp($find);
		if ((defined $find) && ($find ne "") && ($find ne $packagename))
		{
			print "(\033[1;32mFound via search\033[0m) ";
			$foo = "-I$1";
		}
		else
		{
			$foo = " ";
			undef $v;
		}
		$ret = "$foo";
	}
	if (($defaults ne "") && (($ret eq "") || (!defined $ret)))
	{
		$ret = "$foo " . $defaults;
	}
	chomp($ret);
	if ((($ret eq " ") || (!defined $ret)) && ((!defined $v) || ($v eq "")))
	{
		my $key = "default_includedir_$packagename";
		if (exists $main::config{$key})
		{
			$ret = $main::config{$key};
		}
		else
		{
			$headername =~ s/^\///;
			promptstring("path to the directory containing $headername", $key, "/usr/include",$packagename,"$packagename-includes");
			$packagename =~ tr/a-z/A-Z/;
			$main::config{$key} = "-I$main::config{$key}" . " $defaults -DVERSION_$packagename=\"$v\"";
			$main::config{$key} =~ s/^\s+//g;
			$ret = $main::config{$key};
			return $ret;
		}
	}
	else
	{
		chomp($v);
		my $key = "default_includedir_$packagename";
		$packagename =~ tr/a-z/A-Z/;
		$main::config{$key} = "$ret -DVERSION_$packagename=\"$v\"";
		$main::config{$key} =~ s/^\s+//g;
		$ret = $main::config{$key};
		print "\033[1;32m$ret\033[0m (version $v)\n";
	}
	$ret =~ s/^\s+//g;
	return $ret;
}

sub vcheck($$)
{
	my ($version1, $version2) = @_;
	$version1 =~ s/\-r(\d+)/\.\1/g; # minor revs/patchlevels
	$version2 =~ s/\-r(\d+)/\.\1/g;
	$version1 =~ s/p(\d+)/\.\1/g;
	$version2 =~ s/p(\d+)/\.\1/g;
	$version1 =~ s/\-//g;
	$version2 =~ s/\-//g;
	$version1 =~ s/a-z//g;
	$version2 =~ s/a-z//g;
	my @v1 = split('\.', $version1);
	my @v2 = split('\.', $version2);
	for ($curr = 0; $curr < scalar(@v1); $curr++)
	{
		if ($v1[$curr] < $v2[$curr])
		{
			return 0;
		}
	}
	return 1;
}

sub pkgconfig_check_version($$;$)
{
	my ($packagename, $version, $module) = @_;

	extend_pkg_path();

	print "Checking version of package \033[1;32m$packagename\033[0m is >= \033[1;32m$version\033[0m... ";

	$v = `pkg-config --modversion $packagename 2>/dev/null`;
	if (defined $v)
	{
		chomp($v);
	}
	if ((defined $v) && ($v ne ""))
	{
		if (vcheck($v,$version) == 1)
		{
			print "\033[1;32mYes (version $v)\033[0m\n";
			return 1;
		}
		else
		{
			print "\033[1;32mNo (version $v)\033[0m\n";
			return 0;
		}
	}
	# If we didnt find it, we  cant definitively say its too old.
	# Return ok, and let pkgconflibs() or pkgconfincludes() pick up
	# the missing library later on.
	print "\033[1;32mNo (not found)\033[0m\n";
	return 1;
}

sub pkgconfig_get_lib_dirs($$$;$)
{
	my ($packagename, $libname, $defaults, $module) = @_;

	my $key = "default_libdir_$packagename";
	if (exists $main::config{$key})
	{
		print "Locating library directory for package \033[1;32m$packagename\033[0m for module \033[1;32m$module\033[0m... ";
		$ret = $main::config{$key};
		print "\033[1;32m$ret\033[0m (cached)\n";
		return $ret;
	}

	extend_pkg_path();

	print "Locating library directory for package \033[1;32m$packagename\033[0m for module \033[1;32m$module\033[0m... ";

	$v = `pkg-config --modversion $packagename 2>/dev/null`;
	$ret = `pkg-config --libs $packagename 2>/dev/null`;

	if ((!defined $v) || ($v eq ""))
	{
		$foo = `locate "$libname" | head -n 1`;
		$foo =~ /(.+)\Q$libname\E/;
		$find = $1;
		chomp($find);
		if ((defined $find) && ($find ne "") && ($find ne $packagename))
		{
			print "(\033[1;32mFound via search\033[0m) ";
			$foo = "-L$1";
		}
		else
		{
			$foo = " ";
			undef $v;
		}
		$ret = "$foo";
	}

	if (($defaults ne "") && (($ret eq "") || (!defined $ret)))
	{
		$ret = "$foo " . $defaults;
	}
	chomp($ret);
	if ((($ret eq " ") || (!defined $ret)) && ((!defined $v) || ($v eq "")))
	{
		my $key = "default_libdir_$packagename";
		if (exists $main::config{$key})
		{
			$ret = $main::config{$key};
		}
		else
		{
			$libname =~ s/^\///;
			promptstring("path to the directory containing $libname", $key, "/usr/lib",$packagename,"$packagename-libs");
			$main::config{$key} = "-L$main::config{$key}" . " $defaults";
			$main::config{$key} =~ s/^\s+//g;
			$ret = $main::config{$key};
			return $ret;
		}
	}
	else
	{
		chomp($v);
		print "\033[1;32m$ret\033[0m (version $v)\n";
		my $key = "default_libdir_$packagename";
		$main::config{$key} = $ret;
		$main::config{$key} =~ s/^\s+//g;
		$ret =~ s/^\s+//g;
	}
	$ret =~ s/^\s+//g;
	return $ret;
}

# Translate a $CompileFlags etc line and parse out function calls
# to functions within these modules at configure time.
sub translate_functions($$)
{
	my ($line,$module) = @_;

	eval
	{
		$module =~ /modules*\/(.+?)$/;
		$module = $1;

		# This is only a cursory check, just designed to catch casual accidental use of backticks.
		# There are pleanty of ways around it, but its not supposed to be for security, just checking
		# that people are using the new configuration api as theyre supposed to and not just using
		# backticks instead of eval(), being as eval has accountability. People wanting to get around
		# the accountability will do so anyway.
		if (($line =~ /`/) && ($line !~ /eval\(.+?`.+?\)/))
		{
			die "Developers should no longer use backticks in configuration macros. Please use exec() and eval() macros instead. Offending line: $line (In module: $module)";
		}
		while ($line =~ /exec\("(.+?)"\)/)
		{
			print "Executing program for module \033[1;32m$module\033[0m ... \033[1;32m$1\033[0m\n";
			my $replace = `$1`;
			chomp($replace);
			$line =~ s/exec\("(.+?)"\)/$replace/;
		}
		while ($line =~ /execruntime\("(.+?)"\)/)
		{
			$line =~ s/execruntime\("(.+?)"\)/`\1`/;
		}
		while ($line =~ /eval\("(.+?)"\)/)
		{
			print "Evaluating perl code for module \033[1;32m$module\033[0m ... ";
			my $tmpfile;
			do
			{
				$tmpfile = tmpnam();
			} until sysopen(TF, $tmpfile, O_RDWR|O_CREAT|O_EXCL|O_NOFOLLOW, 0700);
			print "(Created and executed \033[1;32m$tmpfile\033[0m)\n";
			print TF $1;
			close TF;
			my $replace = `perl $tmpfile`;
			chomp($replace);
			$line =~ s/eval\("(.+?)"\)/$replace/;
		}
		while ($line =~ /pkgconflibs\("(.+?)","(.+?)","(.+?)"\)/)
		{
			my $replace = pkgconfig_get_lib_dirs($1, $2, $3, $module);
			$line =~ s/pkgconflibs\("(.+?)","(.+?)","(.+?)"\)/$replace/;
		}
		while ($line =~ /pkgconfversion\("(.+?)","(.+?)"\)/)
		{
			if (pkgconfig_check_version($1, $2, $module) != 1)
			{
				die "Version of package $1 is too old. Please upgrade it to version \033[1;32m$2\033[0m or greater and try again.";
			}
			# This doesnt actually get replaced with anything
			$line =~ s/pkgconfversion\("(.+?)","(.+?)"\)//;
		}
		while ($line =~ /pkgconflibs\("(.+?)","(.+?)",""\)/)
		{
			my $replace = pkgconfig_get_lib_dirs($1, $2, "", $module);
			$line =~ s/pkgconflibs\("(.+?)","(.+?)",""\)/$replace/;
		}
		while ($line =~ /pkgconfincludes\("(.+?)","(.+?)",""\)/)
		{
			my $replace = pkgconfig_get_include_dirs($1, $2, "", $module);
			$line =~ s/pkgconfincludes\("(.+?)","(.+?)",""\)/$replace/;
		}
		while ($line =~ /pkgconfincludes\("(.+?)","(.+?)","(.+?)"\)/)
		{
			my $replace = pkgconfig_get_include_dirs($1, $2, $3, $module);
			$line =~ s/pkgconfincludes\("(.+?)","(.+?)","(.+?)"\)/$replace/;
		}
		while ($line =~ /rpath\("(.+?)"\)/)
		{
			my $replace = make_rpath($1,$module);
			$replace = "" if ($^O =~ /darwin/i);
			$line =~ s/rpath\("(.+?)"\)/$replace/;
		}
	};
	if ($@)
	{
		$err = $@;
		$err =~ s/at .+? line \d+.*//g;
		print "\n\nConfiguration failed. The following error occured:\n\n$err\n";
		exit;
	}
	else
	{
		return $line;
	}
}

1;

