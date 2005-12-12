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

my $confpath = "@CONFIG_DIR@/";
my $binpath = "@BINARY_DIR@";
my $libpath = "@LIBRARY_DIR@";
my $executable = "@EXECUTABLE@";
my @filesparsed;

$ENV{"LD_LIBRARY_PATH"} = $ENV{"LD_LIBRARY_PATH"} . ":/usr/local/lib/mysql:/usr/lib/mysql:$libpath";

# Lets see what they want to do.. Set the variable (Cause i'm a lazy coder)
my $arg = $ARGV[0];
getpidfile($confpath."inspircd.conf");

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
        system("$binpath/$executable");
	sleep 2;
        return 1;
}

sub debug {
        # Check to see its not 'running' already.
        if (getstatus() == 1) { print "InspIRCd is already running.\n"; return 0; }
        # If we are still alive here.. Try starting the IRCd..
        system("gdb --args $binpath/$executable -nofork -debug");
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

# GetPidfile Version 2 - Now With Include Support..
# I beg for months for include support in insp, then.. 
# when it is added, it comes around and BITES ME IN THE ASS,
# because i then have to code support into this script.. Evil.

sub getpidfile {
  my ($file) = @_;
  # Before we start, do we have a PID already? (Should never occur)
  if ($pid ne "") {
    return;
  }
  # Are We using a relative path?
  if ($file !~ /^\//) {
    # Convert it to a full path..
    $file = $confpath . $file;
  }

  # Have we checked this file before?
  for (my $i = 0; $i < $filesparsed; $i++) {
    if ($filesparsed[$i] eq $file) {
      # Already Parsed, Possible recursive loop..
      return;
    }
  }
  
  # If we get here, Mark as 'Read'
  $filesparsed[$filesparsed] = $file;

  # Open the File..
  open INFILE, "< $file" or die "Unable to Open file $file\n";
  # Grab entire file contents..
  my(@lines) = <INFILE>;
  # Close the file
  close INFILE;

  # Clean up the file, no newlines etc..
  chomp(@lines);
  foreach $i (@lines) {
    $i =~ s/[^=]+=\s(.*)/\1/;
  }
  my $tmp = join("",@lines);

  # Does this file have a pid?
  if ($tmp =~ /<pid file=\"(\S+)\">/i) {
    # Set the PID file and return.
    $pidfile = $1;
    return;
  }

  # If we get here, NO PID FILE! -- Check for includes (Seeing as we will eventually return,
  # The while (1) is safe.)
  while (1) {
    if ($tmp =~ s/\<include file=\"(.+?)\"\>//i)
    {
      # Decend into that file, and check for PIDs.. (that sounds like an STD ;/)
      getpidfile($1);
      # Was a PID found?
      if ($pidfile ne "") {
        # Yes, Return.
        return;
      }
    } else {
      # End of includes / No includes found.
      return;
    }
  }
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

