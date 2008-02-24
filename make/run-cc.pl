#!/usr/bin/perl

### THIS IS DESIGNED TO BE RUN BY MAKE! DO NOT RUN FROM THE SHELL (because it MIGHT sigterm the shell)! ###

use strict;
use warnings FATAL => qw(all);

use POSIX ();

# Runs the compiler, passing it the given arguments.
# Filters select output from the compiler's standard error channel and
# can take different actions as a result.

# NOTE: this is *NOT* a hash (sadly: a hash would stringize all the regexes and thus render them useless, plus you can't index a hash based on regexes anyway)
# even though we use the => in it.

# The subs are passed the message, and anything the regex captured.

my $cc = shift(@ARGV);

my $showncmdline = 0;

# GCC's "location of error stuff", which accumulates the "In file included from" include stack
my $location = "";

my @msgfilters = (
	[ qr/^(.*) warning: cannot pass objects of non-POD type `(.*)' through `\.\.\.'; call will abort at runtime/ => sub {
		my ($msg, $where, $type) = @_;
		print $location;
		$location = "";
		my $errstr = "$where error: cannot pass objects of non-POD type `$type' through `...'\n";
		if ($type =~ m/::(basic_)?string/) {
			$errstr .= "$where (Did you forget to call c_str()?)\n";
		}
		die $errstr;
	} ],

	# Start of an include stack.
	[ qr/^In file included from .*[,:]$/ => sub {
		my ($msg) = @_;
		$location = "$msg\n";
	} ],

	# Continuation of an include stack.
	[ qr/^                 from .*[,:]$/ => sub {
		my ($msg) = @_;
		$location .= "$msg\n";
	} ],

	# A function, method, constructor, or destructor is the site of a problem
	[ qr/In ((con|de)structor|(member )?function)/ => sub {
		my ($msg) = @_;
		# If a complete location string is waiting then probably we dropped an error, so drop the location for a new one.
		if ($location =~ m/In ((con|de)structor|(member )?function)/) {
			$location = "$msg\n";
		} else {
			$location .= "$msg\n";
		}
	} ],

	[ qr/^.* warning: / => sub {
		my ($msg) = @_;
		print $location;
		$location = "";
		print STDERR "\e[33;1m$msg\e[0m\n";
	} ],

	[ qr/^.* error: / => sub {
		my ($msg) = @_;
		print STDERR "An error occured when executing:\e[37;1m $cc " . join(' ', @ARGV) . "\n" unless $showncmdline;
		$showncmdline = 1;
		print $location;
		$location = "";
		print STDERR "\e[31;1m$msg\e[0m\n";
	} ],
);

my $pid;

my ($r_stderr, $w_stderr);

my $name = "";
my $action = "";

if ($cc eq "ar") {
	$name = $ARGV[1];
	$action = "ARCHIVE";
} else {
	foreach my $n (@ARGV)
	{
		if ($n =~ /\.cpp$/)
		{
			if ($action eq "BUILD")
			{
				$name .= " " . $n;
			}
			else
			{
				$action = "BUILD";
				$name = $n;
			}
		}
		elsif ($action eq "BUILD") # .cpp has priority.
		{
			next;
		}
		elsif ($n eq "-o")
		{
			$action = $name = $n;
		}
		elsif ($name eq "-o")
		{
			$action = "LINK";
			$name = $n;
		}
	}
}

if (!defined($cc) || $cc eq "") {
	die "Compiler not specified!\n";
}

pipe($r_stderr, $w_stderr) or die "pipe stderr: $!\n";

$pid = fork;

die "Cannot fork to start $cc! $!\n" unless defined($pid);

if ($pid) {

	printf "\t\e[1;32m%-20s\e[0m%s\n", $action . ":", $name unless $name eq "";

	my $fail = 0;
	# Parent - Close child-side pipes.
	close $w_stderr;
	# Close STDIN to ensure no conflicts with child.
	close STDIN;
	# Now read each line of stderr
LINE:	while (defined(my $line = <$r_stderr>)) {
		chomp $line;
		for my $filter (@msgfilters) {
			my @caps;
			if (@caps = ($line =~ $filter->[0])) {
				$@ = "";
				eval {
					$filter->[1]->($line, @caps);
				};
				if ($@) {
					$fail = 1;
					print STDERR $@;
				}
				next LINE;
			}
		}
		print STDERR "$line\n";
	}
	waitpid $pid, 0;
	close $r_stderr;
	my $exit = $?;
	# Simulate the same exit, so make gets the right termination info.
	if (POSIX::WIFSIGNALED($exit)) {
		# Make won't get the right termination info (it gets ours, not the compiler's), so we must tell the user what really happened ourselves!
		print STDERR "$cc killed by signal " . POSIX::WTERMSIGN($exit) . "\n";
		kill "TERM", getppid(); # Needed for bsd make.
		kill "TERM", $$;
	}
	else {
		if (POSIX::WEXITSTATUS($exit) == 0) {
			if ($fail) {
				kill "TERM", getppid(); # Needed for bsd make.
				kill "TERM", $$;
			}
			exit 0;
		} else {
			exit POSIX::WEXITSTATUS($exit);
		}
	}
} else {
	# Child - Close parent-side pipes.
	close $r_stderr;
	# Divert stderr
	open STDERR, ">&", $w_stderr or die "Cannot divert STDERR: $!\n";
	# Run the compiler!
	exec { $cc } $cc, @ARGV;
	die "exec $cc: $!\n";
}
