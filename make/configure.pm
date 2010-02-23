#       +------------------------------------+
#       | Inspire Internet Relay Chat Daemon |
#       +------------------------------------+
#
#  InspIRCd: (C) 2002-2010 InspIRCd Development Team
# See: http://wiki.inspircd.org/Credits
#
# This program is free but copyrighted software; see
#      the file COPYING for details.
#
# ---------------------------------------------------

package make::configure;

require 5.8.0;

use strict;
use warnings FATAL => qw(all);

use Exporter 'import';
use POSIX;
use make::utilities;
our @EXPORT = qw(promptnumeric dumphash is_dir getmodules getrevision getcompilerflags getlinkerflags getdependencies nopedantic resolve_directory yesno showhelp promptstring_s);

my $no_svn = 0;

sub yesno {
	my ($flag,$prompt) = @_;
	print "$prompt [\e[1;32m$main::config{$flag}\e[0m] -> ";
	chomp(my $tmp = <STDIN>);
	if ($tmp eq "") { $tmp = $main::config{$flag} }
	if (($tmp eq "") || ($tmp =~ /^y/i))
	{
		$main::config{$flag} = "y";
	}
	else
	{
		$main::config{$flag} = "n";
	}
	return;
}

sub resolve_directory
{
	my $ret = $_[0];
	eval
	{
		use File::Spec;
		$ret = File::Spec->rel2abs($_[0]);
	};
	return $ret;
}

sub getrevision {
	if ($no_svn)
	{
		return "0";
	}
	my $data = `svn info 2>/dev/null`;
	if ($data eq "")
	{
		$data = `git describe --tags 2>/dev/null`;
		if ($data eq "")
		{
			$no_svn = 1;
			return '0';
		}
		chomp $data; # remove \n
		return $data;
	}
	$data =~ /Revision: (\d+)/;
	my $rev = $1;
	if (!defined($rev))
	{
		$rev = "0";
	}
	return $rev;
}

sub getcompilerflags {
	my ($file) = @_;
	open(FLAGS, $file) or return "";
	while (<FLAGS>) {
		if ($_ =~ /^\/\* \$CompileFlags: (.+) \*\/$/) {
			my $x = translate_functions($1, $file);
			next if ($x eq "");
			close(FLAGS);
			return $x;
		}
	}
	close(FLAGS);
	return "";
}

sub getlinkerflags {
	my ($file) = @_;
	open(FLAGS, $file) or return "";
	while (<FLAGS>) {
		if ($_ =~ /^\/\* \$LinkerFlags: (.+) \*\/$/) {
			my $x = translate_functions($1, $file);
			next if ($x eq "");
			close(FLAGS);
			return $x;
		}
	}
	close(FLAGS);
	return "";
}

sub getdependencies {
	my ($file) = @_;
	open(FLAGS, $file) or return "";
	while (<FLAGS>) {
		if ($_ =~ /^\/\* \$ModDep: (.+) \*\/$/) {
			my $x = translate_functions($1, $file);
			next if ($x eq "");
			close(FLAGS);
			return $x;
		}
	}
	close(FLAGS);
	return "";
}

sub nopedantic {
	my ($file) = @_;
	open(FLAGS, $file) or return "";
	while (<FLAGS>) {
		if ($_ =~ /^\/\* \$NoPedantic \*\/$/) {
			my $x = translate_functions($_, $file);
			next if ($x eq "");
			close(FLAGS);
			return 1;
		}
	}
	close(FLAGS);
	return 0;
}

sub getmodules
{
	my ($silent) = @_;

	my $i = 0;

	if (!$silent)
	{
		print "Detecting modules ";
	}

	opendir(DIRHANDLE, "src/modules") or die("WTF, missing src/modules!");
	foreach my $name (sort readdir(DIRHANDLE))
	{
		if ($name =~ /^m_(.+)\.cpp$/)
		{
			my $mod = $1;
			$main::modlist[$i++] = $mod;
			if (!$silent)
			{
				print ".";
			}
		}
	}
	closedir(DIRHANDLE);

	if (!$silent)
	{
		print "\nOk, $i modules.\n";
	}
}

sub promptnumeric($$)
{
	my $continue = 0;
	my ($prompt, $configitem) = @_;
	while (!$continue)
	{
		print "Please enter the maximum $prompt?\n";
		print "[\e[1;32m$main::config{$configitem}\e[0m] -> ";
		chomp(my $var = <STDIN>);
		if ($var eq "")
		{
			$var = $main::config{$configitem};
		}
		if ($var =~ /^\d+$/) {
			# We don't care what the number is, set it and be on our way.
			$main::config{$configitem} = $var;
			$continue = 1;
			print "\n";
		} else {
			print "You must enter a number in this field. Please try again.\n\n";
		}
	}
}

sub promptstring_s($$)
{
	my ($prompt,$default) = @_;
	my $var;
	print "$prompt\n";
	print "[\e[1;32m$default\e[0m] -> ";
	chomp($var = <STDIN>);
	$var = $default if $var eq "";
	print "\n";
	return $var;
}

sub dumphash()
{
	print "\n\e[1;32mPre-build configuration is complete!\e[0m\n\n";
	print "\e[0mBase install path:\e[1;32m\t\t$main::config{BASE_DIR}\e[0m\n";
	print "\e[0mConfig path:\e[1;32m\t\t\t$main::config{CONFIG_DIR}\e[0m\n";
	print "\e[0mModule path:\e[1;32m\t\t\t$main::config{MODULE_DIR}\e[0m\n";
	print "\e[0mGCC Version Found:\e[1;32m\t\t$main::config{GCCVER}.$main::config{GCCMINOR}\e[0m\n";
	print "\e[0mCompiler program:\e[1;32m\t\t$main::config{CC}\e[0m\n";
	print "\e[0mGnuTLS Support:\e[1;32m\t\t\t$main::config{USE_GNUTLS}\e[0m\n";
	print "\e[0mOpenSSL Support:\e[1;32m\t\t$main::config{USE_OPENSSL}\e[0m\n\n";
	print "\e[1;32mImportant note: The maximum length values are now configured in the\e[0m\n";
	print "\e[1;32m                configuration file, not in ./configure! See the <limits>\e[0m\n";
	print "\e[1;32m                tag in the configuration file for more information.\e[0m\n\n";
}

sub is_dir
{
	my ($path) = @_;
	if (chdir($path))
	{
		chdir($main::this);
		return 1;
	}
	else
	{
		# Just in case..
		chdir($main::this);
		return 0;
	}
}

sub showhelp
{
	chomp(my $PWD = `pwd`);
	print "Usage: configure [options]

*** NOTE: NON-INTERACTIVE CONFIGURE IS *NOT* SUPPORTED BY THE ***
*** INSPIRCD DEVELOPMENT TEAM. DO NOT ASK FOR HELP REGARDING  ***
***     NON-INTERACTIVE CONFIGURE ON THE FORUMS OR ON IRC!    ***

Options: [defaults in brackets after descriptions]

When no options are specified, interactive
configuration is started and you must specify
any required values manually. If one or more
options are specified, non-interactive configuration
is started, and any omitted values are defaulted.

Arguments with a single \"-\" symbol, as in
InspIRCd 1.0.x, are also allowed.

  --disable-interactive        Sets no options itself, but
                               will disable any interactive prompting.
  --disable-rpath              Disable runtime paths. DO NOT USE UNLESS
                               YOU KNOW WHAT YOU ARE DOING!
  --update                     Update makefiles and dependencies
  --modupdate                  Detect new modules and write makefiles
  --svnupdate {--rebuild}      Update working copy via subversion
                                {and optionally rebuild if --rebuild
                                 is also specified}
  --clean                      Remove .config.cache file and go interactive
  --enable-gnutls              Enable GnuTLS module [no]
  --enable-openssl             Enable OpenSSL module [no]
  --enable-optimization=[n]    Optimize using -O[n] gcc flag
  --enable-epoll               Enable epoll() where supported [set]
  --enable-kqueue              Enable kqueue() where supported [set]
  --disable-epoll              Do not enable epoll(), fall back
                               to select() [not set]
  --disable-kqueue             Do not enable kqueue(), fall back
                               to select() [not set]
  --disable-ipv6               Do not build ipv6 native InspIRCd [not set]
  --with-cc=[filename]         Use an alternative g++ binary to
                               build InspIRCd [g++]
  --with-maxbuf=[n]            Change the per message buffer size [512]
                               DO NOT ALTER THIS OPTION WITHOUT GOOD REASON
                               AS IT *WILL* BREAK CLIENTS!!!
  --prefix=[directory]         Base directory to install into (if defined,
                               can automatically define config, module, bin
			       and library dirs as subdirectories of prefix)
                               [$PWD]
  --config-dir=[directory]     Config file directory for config and SSL certs
                               [$PWD/conf]
  --module-dir=[directory]     Modules directory for loadable modules
                               [$PWD/modules]
  --binary-dir=[directory]     Binaries directory for core binary
                               [$PWD/bin]
  --library-dir=[directory]    Library directory for core libraries
                               [$PWD/lib]
  --list-extras                Show current status of extra modules
  --enable-extras=[extras]     Enable the specified list of extras
  --disable-extras=[extras]    Disable the specified list of extras
  --help                       Show this help text and exit

";
	exit(0);
}

1;

