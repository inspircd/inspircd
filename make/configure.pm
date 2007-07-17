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

package make::configure;
use Exporter 'import';
use POSIX;
use make::utilities;
@EXPORT = qw(promptnumeric dumphash is_dir getmodules getrevision getcompilerflags getlinkerflags getdependencies resolve_directory yesno showhelp promptstring_s);

my $no_svn = 0;

sub yesno {
	my ($flag,$prompt) = @_;
	print "$prompt [\033[1;32m$main::config{$flag}\033[0m] -> ";
	chomp($tmp = <STDIN>);
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
	my $data = `svn info`;
	if ($data eq "")
	{
		$no_svn = 1;
		$rev = "0";
		return $rev;
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
	open(FLAGS, $file);
	while (<FLAGS>) {
		if ($_ =~ /^\/\* \$CompileFlags: (.+) \*\/$/) {
			close(FLAGS);
			return translate_functions($1,$file);
		}
	}
	close(FLAGS);
	return undef;
}

sub getlinkerflags {
	my ($file) = @_;
	open(FLAGS, $file);
	while (<FLAGS>) {
		if ($_ =~ /^\/\* \$LinkerFlags: (.+) \*\/$/) {
			close(FLAGS);
			return translate_functions($1,$file);
		}
	}
	close(FLAGS);
	return undef;
}

sub getdependencies {
	my ($file) = @_;
	open(FLAGS, $file);
	while (<FLAGS>) {
		if ($_ =~ /^\/\* \$ModDep: (.+) \*\/$/) {
			close(FLAGS);
			return translate_functions($1,$file);
		}
	}
	close(FLAGS);
	return undef;
}


sub getmodules
{
	my $i = 0;
	print "Detecting modules ";
	opendir(DIRHANDLE, "src/modules");
	foreach $name (sort readdir(DIRHANDLE))
	{
		if ($name =~ /^m_(.+)\.cpp$/)
		{
			$mod = $1;
			if ($mod !~ /_static$/)
			{
				$main::modlist[$i++] = $mod;
				print ".";
			}
		}
	}
	closedir(DIRHANDLE);
	print "\nOk, $i modules.\n";
}

sub promptnumeric($$)
{
	my $continue = 0;
	my ($prompt, $configitem) = @_;
	while (!$continue)
	{
		print "Please enter the maximum $prompt?\n";
		print "[\033[1;32m$main::config{$configitem}\033[0m] -> ";
		chomp($var = <STDIN>);
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
	print "[\033[1;32m$default\033[0m] -> ";
	chomp($var = <STDIN>);
	$var = $default if $var eq "";
	print "\n";
	return $var;
}

sub dumphash()
{
	print "\n\033[1;32mPre-build configuration is complete!\033[0m\n\n";
	print "\033[0mBase install path:\033[1;32m\t\t$main::config{BASE_DIR}\033[0m\n";
	print "\033[0mConfig path:\033[1;32m\t\t\t$main::config{CONFIG_DIR}\033[0m\n";
	print "\033[0mModule path:\033[1;32m\t\t\t$main::config{MODULE_DIR}\033[0m\n";
	print "\033[0mLibrary path:\033[1;32m\t\t\t$main::config{LIBRARY_DIR}\033[0m\n";
	print "\033[0mMax connections:\033[1;32m\t\t$main::config{MAX_CLIENT}\033[0m\n";
	print "\033[0mMax nickname length:\033[1;32m\t\t$main::config{NICK_LENGT}\033[0m\n";
	print "\033[0mMax channel length:\033[1;32m\t\t$main::config{CHAN_LENGT}\033[0m\n";
	print "\033[0mMax mode length:\033[1;32m\t\t$main::config{MAXI_MODES}\033[0m\n";
	print "\033[0mMax ident length:\033[1;32m\t\t$main::config{MAX_IDENT}\033[0m\n";
	print "\033[0mMax quit length:\033[1;32m\t\t$main::config{MAX_QUIT}\033[0m\n";
	print "\033[0mMax topic length:\033[1;32m\t\t$main::config{MAX_TOPIC}\033[0m\n";
	print "\033[0mMax kick length:\033[1;32m\t\t$main::config{MAX_KICK}\033[0m\n";
	print "\033[0mMax name length:\033[1;32m\t\t$main::config{MAX_GECOS}\033[0m\n";
	print "\033[0mMax away length:\033[1;32m\t\t$main::config{MAX_AWAY}\033[0m\n";
	print "\033[0mGCC Version Found:\033[1;32m\t\t$main::config{GCCVER}.x\033[0m\n";
	print "\033[0mCompiler program:\033[1;32m\t\t$main::config{CC}\033[0m\n";
	print "\033[0mStatic modules:\033[1;32m\t\t\t$main::config{STATIC_LINK}\033[0m\n";
	print "\033[0mIPv6 Support:\033[1;32m\t\t\t$main::config{IPV6}\033[0m\n";
	print "\033[0mIPv6 to IPv4 Links:\033[1;32m\t\t$main::config{SUPPORT_IP6LINKS}\033[0m\n";
	print "\033[0mGnuTLS Support:\033[1;32m\t\t\t$main::config{USE_GNUTLS}\033[0m\n";
	print "\033[0mOpenSSL Support:\033[1;32m\t\t$main::config{USE_OPENSSL}\033[0m\n\n";
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
	chomp($PWD = `pwd`);
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

  --disable-interactive        Sets no options intself, but
                               will disable any interactive prompting.
  --update                     Update makefiles and dependencies
  --modupdate                  Detect new modules and write makefiles
  --svnupdate {--rebuild}      Update working copy via subversion
                                {and optionally rebuild if --rebuild
                                 is also specified}
  --clean                      Remove .config.cache file and go interactive
  --enable-gnutls              Enable GnuTLS module [no]
  --enable-openssl             Enable OpenSSL module [no]
  --with-nick-length=[n]       Specify max. nick length [32]
  --with-channel-length=[n]    Specify max. channel length [64]
  --with-max-clients=[n]       Specify maximum number of users
                               which may connect locally
  --enable-optimization=[n]    Optimize using -O[n] gcc flag
  --enable-epoll               Enable epoll() where supported [set]
  --enable-kqueue              Enable kqueue() where supported [set]
  --disable-epoll              Do not enable epoll(), fall back
                               to select() [not set]
  --disable-kqueue             Do not enable kqueue(), fall back
                               to select() [not set]
  --enable-ipv6                Build ipv6 native InspIRCd [no]
  --enable-remote-ipv6         Build with ipv6 support for remote
                               servers on the network [yes]
  --disable-remote-ipv6        Do not allow remote ipv6 servers [not set]
  --with-cc=[filename]         Use an alternative g++ binary to
                               build InspIRCd [g++]
  --with-ident-length=[n]      Specify max length of ident [12]
  --with-quit-length=[n]       Specify max length of quit [200]
  --with-topic-length=[n]      Specify max length of topic [350]
  --with-kick-length=[n]       Specify max length of kick [200]
  --with-gecos-length=[n]      Specify max length of gecos [150]
  --with-away-length=[n]       Specify max length of away [150]
  --with-max-modes=[n]         Specify max modes per line which
                               have parameters [20]
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
  --help                       Show this help text and exit

";
	exit(0);
}

1;

