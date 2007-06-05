#!/usr/bin/perl
#       +------------------------------------+
#       | Inspire Internet Relay Chat Daemon |
#       +------------------------------------+
#
#      (C) 2002-2007 InspIRCd Development Team
#   http://www.inspircd.org/wiki/index.php/Credits
#
# Written by Craig Edwards, Craig McLure, and others.
# This program is free but copyrighted software; see
#            the file COPYING for details.
#
# ---------------------------------------------------
use POSIX;

my $basepath	=	"@BASE_DIR@";
my $confpath	=	"@CONFIG_DIR@/";
my $binpath	=	"@BINARY_DIR@";
my $libpath	=	"@LIBRARY_DIR@";
my $executable	=	"@EXECUTABLE@";
my $version	=	"@VERSION@";
my @filesparsed;
my @filechecked;

# Lets see what they want to do.. Set the variable (Cause i'm a lazy coder)
my $arg = $ARGV[0];
getpidfile($confpath."inspircd.conf");

if ($arg eq "start") { start(); exit(); }
if ($arg eq "debug") { debug(); exit(); }
if ($arg eq "screendebug") { screendebug(); exit() }
if ($arg eq "valdebug") { valdebug(); exit(); }
if ($arg eq "screenvaldebug") { screenvaldebug(); exit(); }
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
		print "InspIRCd rehashed (pid: $pid).\n";
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

if ($arg eq "version") {
	print "InspIRCd version: $version\n";
	exit();
}

if ($arg eq "restart") {
	stop();
	unlink($pidfile) if (-e $pidfile);
	start();
	# kthxbye();
	exit();
}

if ($arg eq "checkconf") {
	checkconf();
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
print "Usage: inspircd (start|stop|restart|rehash|status|cron|checkconf|version)\n";
print "Developer arguments: (debug|screendebug|valdebug|screenvaldebug)\n";
exit();

###
# Generic Helper Functions.
###

sub start {
	# Check to see its not 'running' already.
	if (getstatus() == 1) { print "InspIRCd is already running.\n"; return 0; }
	# If we are still alive here.. Try starting the IRCd..
	print "$binpath/$executable doesn't exist\n" and return 0 unless(-e "$binpath/$executable");

	system("$binpath/$executable");
	return 1;
}

sub debug {
	# Check to see its not 'running' already.
	if (getstatus() == 1) { print "InspIRCd is already running.\n"; return 0; }

	print "$binpath/$executable doesn't exist\n" and return 0 unless(-e "$binpath/$executable");

	# Check we have gdb
	checkgdb();

	# If we are still alive here.. Try starting the IRCd..
	system("gdb --command=$basepath/.gdbargs --args $binpath/$executable -nofork -debug -nolog");
}

sub screendebug
{
	# Check to see its not 'running' already.
	if (getstatus() == 1) { print "InspIRCd is already running.\n"; return 0; }

	print "$binpath/$executable doesn't exist\n" and return 0 unless(-e "$binpath/$executable");

	#Check we have gdb
	checkgdb();
	checkscreen();

	# If we are still alive here.. Try starting the IRCd..
	print "Starting InspIRCd in `screen`, type `screen -r` when the ircd crashes to view the gdb output and get a backtrace.\n";
	print "Once you're inside the screen session press ^C + d to re-detach from the session\n";
	system("screen -m -d gdb --command=$basepath/.gdbargs --args $binpath/$executable -nofork -debug -nolog");
}

sub valdebug
{
	# Check to see its not 'running' already.
	if (getstatus() == 1) { print "InspIRCd is already running.\n"; return 0; }

	print "$binpath/$executable doesn't exist\n" and return 0 unless(-e "$binpath/$executable");

	# Check we have valgrind and gdb
	checkvalgrind();
	checkgdb();

	# If we are still alive here.. Try starting the IRCd..
	# May want to do something with these args at some point: --suppressions=.inspircd.sup --gen-suppressions=yes
	# Could be useful when we want to stop it complaining about things we're sure aren't issues.
	system("valgrind -v --tool=memcheck --leak-check=yes --db-attach=yes --num-callers=10 $binpath/$executable -nofork -debug -nolog");
}

sub screenvaldebug
{
	# Check to see its not 'running' already.
	if (getstatus() == 1) { print "InspIRCd is already running.\n"; return 0; }

	print "$binpath/$executable doesn't exist\n" and return 0 unless(-e "$binpath/$executable");

	#Check we have gdb
	checkvalgrind();
	checkgdb();
	checkscreen();

	# If we are still alive here.. Try starting the IRCd..
	print "Starting InspIRCd in `screen`, type `screen -r` when the ircd crashes to view the valgrind and gdb output and get a backtrace.\n";
	print "Once you're inside the screen session press ^C + d to re-detach from the session\n";
	system("screen -m -d valgrind -v --tool=memcheck --leak-check=yes --db-attach=yes --num-callers=10 $binpath/$executable -nofork -debug -nolog");
}

sub stop {
	if (getstatus() == 0) { print "InspIRCd is not running. (Or PID File not found)\n"; return 0; }
	# Get to here, we have something to kill.
	my $pid = getprocessid();
	print "Stopping InspIRCd (pid: $pid)...\n";
	system("kill -TERM $pid >/dev/null 2>&1");
	# Give it a second to exit
	sleep(1);
	if (getstatus() == 1)
	{
		print "InspIRCd not dying quietly -- forcing kill\n";
		system("kill -9 $pid >/dev/null 2>&1");
	}
	print "InspIRCd Stopped.\n";
}

# GetPidfile Version 2 - Now With Include Support..
# I beg for months for include support in insp, then..
# when it is added, it comes around and BITES ME IN THE ASS,
# because i then have to code support into this script.. Evil.

# Craig got bitten in the ass again --
# in 1.1 beta the include file is manditory, therefore
# if we cant find it, default to %conf%/inspircd.pid.
# Note, this also contains a fix for when the pid file is
# defined, but defined in a comment (line starts with #)
# -- Brain

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
  open INFILE, "< $file" or die "Unable to open file $file\n";
  # Grab entire file contents..
  my(@lines) = <INFILE>;
  # Close the file
  close INFILE;

  # remove trailing spaces
  chomp(@lines);
  foreach $i (@lines) {
    # clean it up
    $i =~ s/[^=]+=\s(.*)/\1/;
    # Does this file have a pid?
    if (($i =~ /<pid file=\"(\S+)\">/i) && ($i !~ /^#/))
    {
      # Set the PID file and return.
      $pidfile = $1;
      return;
    }
  }

  # If we get here, NO PID FILE! -- Check for includes
  foreach $i (@lines) {
    $i =~ s/[^=]+=\s(.*)/\1/;
    if (($i =~ s/\<include file=\"(.+?)\"\>//i) && ($i !~ /^#/))
    {
      # Decend into that file, and check for PIDs.. (that sounds like an STD ;/)
      getpidfile($1);
      # Was a PID found?
      if ($pidfile ne "") {
	# Yes, Return.
	return;
      }
    }
  }

  # End of includes / No includes found. Using default.
  $pidfile = $confpath . "inspircd.pid";
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

sub checkvalgrind
{
	unless(`valgrind --version`)
	{
		print "Couldn't start valgrind: $!\n";
		exit;
	}
}

sub checkgdb
{
	unless(`gdb --version`)
	{
		print "Couldn't start gdb: $!\n";
		exit;
	}
}

sub checkscreen
{
	unless(`screen --version`)
	{
		print "Couldn't start screen: $!\n";
		exit;
	}
}

sub checkxmllint
{
	open(FH, "xmllint|") or die "Couldn't start xmllint: $!\n";
}

sub checkconf
{
	checkxmllint();
	validateconf($confpath."inspircd.conf");
	print "Config check complete\n";
}

sub validateconf
{
	my ($file) = @_;

	# Are We using a relative path?
	if ($file !~ /^\//) {
		# Convert it to a full path..
		$file = $confpath . $file;
	}

	# Have we checked this file before?
	for (my $i = 0; $i < $filechecked; $i++) {
		if ($filechecked[$i] eq $file) {
			# Already Parsed, Possible recursive loop..
			return;
		}
	}

	# If we get here, Mark as 'Read'
	$filechecked[$filechecked] = $file;

	# Open the File..
	open INFILE, "< $file" or die "Unable to open file $file\n";
	# Grab entire file contents..
	my(@lines) = <INFILE>;
	# Close the file
	close INFILE;

	# remove trailing spaces
	chomp(@lines);

	my @newlines = ();
	my @blanks = ();
	my $conline;

	push @newlines, "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>";
#       push @newlines, "<!DOCTYPE config SYSTEM \"".$confpath."inspircd.dtd\">";
	push @newlines, "<config>";

	foreach $i (@lines)
	{
		# remove trailing newlines
		chomp($i);

		# convert tabs to spaces
		$i =~ s/\t/ /g;

		# remove leading spaces
		$i =~ s/^ *//;

		# remove comments
		$i =~ s/^#.*//;

		# remove trailing #s
		$i =~ s/(.*)#$/\1/;

		# remove trailing comments
		my $line = "";
		my $quote = 0;
		for (my $j = 0; $j < length($i); $j++)
		{
			if (substr($i,$j, 1) eq '"') { $quote = ($quote) ? 0 : 1; } elsif (substr($i,$j, 1) eq "#" && !$quote) { last; }
			$line .= substr($i,$j, 1);
		}
		$i = $line;

		# remove trailing spaces
		$i =~ s/ *$//;

		# setup incf for include check and clean it up, since this breaks parsing use local var
		my $incf = $i;
		$incf =~ s/[^=]+=\s(.*)/\1/;

		# include file?
		if (($incf =~ s/\<include file=\"(.+?)\"\>//i) && ($incf !~ /^#/))
		{
			# yes, process it
			validateconf($1);
		}

		if ($i =~ /^<.*/ && $conline =~ /^<.*/)
		{
			push @newlines, $conline;
			push @newlines, @blanks;
			$conline = $i;
		}

		if ($i =~ /^<.*>$/)
		{
			$i =~ s/(.*)>$/\1 \/>/;
			push @newlines, $i;
		}
		elsif ($i =~ /.*>$/)
		{
			$conline .= " $i";
			$conline =~ s/(.*)>$/\1 \/>/;
			push @blanks, "";
			push @newlines, $conline;
			push @newlines, @blanks;
			$conline = "";
			undef @blanks;
		}
		elsif ($i =~ /^<.*/)
		{
			$conline = $i;
		}
		elsif ($conline =~ /^<.*/ && $i)
		{
			$conline .= " $i";
			push @blanks, "";
		}
		else
		{
			if ($conline)
			{
				push @blanks, $i;
			}
			else
			{
				push @newlines, $i;
			}
		}
	}
	if ($conline)
	{
		push @newlines, $conline;
		push @newlines, @blanks;
	}

	push @newlines, "</config>";

	my $tmpfile;
	do
	{
		$tmpfile = tmpnam();
	} until sysopen(TF, $tmpfile, O_RDWR|O_CREAT|O_EXCL|O_NOFOLLOW, 0700);

	foreach $n (@newlines)
	{
		print TF "$n\n";
	}
	close TF;

	my @result = `xmllint -noout $tmpfile 2>&1`;
	chomp(@result);

	my $skip = 0;
	foreach $n (@result)
	{
		if ($skip)
		{
			$skip = 0;
			next;
		}
		$n =~ s/$tmpfile\:\d*\: *//g;
		if ($n =~ /.*config>.*/)
		{
			$n = "";
			$skip = 1;
		}

		if ($n && !$skip)
		{
			if ($n =~ /line \d*/)
			{
				my $lineno = $n;
				$lineno =~ s/.*line (\d*).*/\1/;
				$lineno = $lineno-2;
				$n =~ s/line (\d*)/line $lineno/;
			}
			print "$file : $n\n";
		}
	}
	unlink($tmpfile);
}
