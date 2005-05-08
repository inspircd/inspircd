#!/usr/bin/perl
#       +------------------------------------+
#       | Inspire Internet Relay Chat Daemon |
#       +------------------------------------+
#
#  Inspire is copyright (C) 2002-2004 ChatSpike-Dev.
#                       E-mail:
#                <brain@chatspike.net>
#                <Craig@chatspike.net>
#
# Written by Craig Edwards, Craig McLure, and others.
# This program is free but copyrighted software; see
#            the file COPYING for details.
#
#               I HATE PERL.. kthxbye
# ---------------------------------------------------

my $conffile = "@CONFIG_DIR@/inspircd.conf";
my $binpath = "@BINARY_DIR@";

$ENV{"LD_LIBRARY_PATH"} = $ENV{"LD_LIBRARY_PATH"} . ":/usr/local/lib/mysql:/usr/lib/mysql:$binpath";

# Lets see what they want to do.. Set the variable (Cause i'm a lazy coder)
my $arg = $ARGV[0];
getpidfile();

if ($arg eq "start") { start(); exit(); }
if ($arg eq "debug") { debug(); exit(); }
if ($arg eq "stop") { stop(); exit(); }
if ($arg eq "status") {
	if (getstatus() == 1) { 
		my $pid = getprocessid();
		print "InspIRCd is running (PID: $pid)\n";
		exit();
	} else {
		print "InspIRCd is not running. (Or PID File not found)\n";
		exit();
	}
}
if ($arg eq "rehash") {
	if (getstatus() == 1) {
		my $pid = getprocessid();
	        system("kill -HUP $pid >/dev/null 2>&1");
		print "InspIRCd rehashed.\n";
		exit();
	} else {
		print "InspIRCd is not running. (Or PID File not found)\n";
		exit();
	}
}

if ($arg eq "cron") {
	if (getstatus() == 0) { start(); }
	exit();
}

if ($arg eq "restart") {
	stop();
	start();
	# kthxbye();
	exit();
}

if ($arg eq "Cheese-Sandwich") {
	print "Creating Cheese Sandwich..\n";
	print "Done.\n";
	exit();
}

###
# If we get here.. bad / no parameters.
###
print "Invalid Argument: $arg\n";
print "Usage: inspircd (start|stop|restart|rehash|status|cron)\n";
exit();

###
# Generic Helper Functions.
###

sub start {
        # Check to see its not 'running' already.
        if (getstatus() == 1) { print "InspIRCd is already running.\n"; return 0; }
        # If we are still alive here.. Try starting the IRCd..
        system("$binpath/inspircd");
        sleep 1;
        if (getstatus() == 0) {
                print "InspIRCd Seemingly not started, Log follows:\n";
                system("tail $binpath/ircd.log");
        } else {
                # We're good!
                return 1;
        }
}

sub debug {
        # Check to see its not 'running' already.
        if (getstatus() == 1) { print "InspIRCd is already running.\n"; return 0; }
        # If we are still alive here.. Try starting the IRCd..
        system("gdb --args $binpath/inspircd -nofork -debug");
}


sub stop {
        if (getstatus() == 0) { print "InspIRCd is not running. (Or PID File not found)\n"; return 0; }
        # Get to here, we have something to kill.
        my $pid = getprocessid();
	print "Stopping InspIRCd...\n";
        system("kill -TERM $pid >/dev/null 2>&1");
	sleep 2;
	if (getstatus() == 1)
	{
		print "InspIRCd not dying Quietly -- Forcing Kill\n";
		system("kill -9 $pid >/dev/null 2>&1");
	}
        print "InspIRCd Stopped.\n";
}


sub getpidfile {
	open INFILE, "<  $conffile" or die "Couldn't open $conffile\n";
	my(@lines) = <INFILE>;
	close INFILE;
	chomp(@lines);

	foreach $i (@lines) {
	    $i =~ s/[^=]+=\s(.*)/\1/;
	}

	my $tmp = join("",@lines);
	$tmp =~ /<pid file=\"(\S+)\">/i;

	$pidfile = $1;
}

sub getstatus {
        my $pid = getprocessid();
        if ($pid == 0) { return 0; }
        $status = system("kill -0 $pid >/dev/null 2>&1") / 256;
        if ($status == 0) { return 1; }
        else { return 0; }
}


sub getprocessid {
	my $pid;
	open PIDFILE, "< $pidfile" or return 0;
	while($i = <PIDFILE>)
	{
		$pid = $i;
	}
	close PIDFILE;
	return $pid;
}
