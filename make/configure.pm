package make::configure;
use Exporter 'import';
use POSIX;
@EXPORT = qw(promptnumeric dumphash);

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

sub dumphash()
{
	print "\n\033[1;32mPre-build configuration is complete!\033[0m\n\n";
	print "\033[0mBase install path:\033[1;32m\t\t$main::config{BASE_DIR}\033[0m\n";
	print "\033[0mConfig path:\033[1;32m\t\t\t$main::config{CONFIG_DIR}\033[0m\n";
	print "\033[0mModule path:\033[1;32m\t\t\t$main::config{MODULE_DIR}\033[0m\n";
	print "\033[0mLibrary path:\033[1;32m\t\t\t$main::config{LIBRARY_DIR}\033[0m\n";
	print "\033[0mMax connections:\033[1;32m\t\t$main::config{MAX_CLIENT}\033[0m\n";
	print "\033[0mMax User Channels:\033[1;32m\t\t$main::config{MAX_CHANNE}\033[0m\n";
	print "\033[0mMax Oper Channels:\033[1;32m\t\t$main::config{MAX_OPERCH}\033[0m\n";
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

1;
