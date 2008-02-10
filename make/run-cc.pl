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

my @msgfilters = (
	[ qr/^(.*) warning: cannot pass objects of non-POD type `(.*)' through `\.\.\.'; call will abort at runtime/ => sub {
		my ($msg, $where, $type) = @_;
		my $errstr = "$where error: cannot pass objects of non-POD type `$type' through `...'\n";
		if ($type =~ m/::string/) {
			$errstr .= "$where (Did you forget to call c_str()?)\n";
		}
		die $errstr;
	} ],

	[ qr/^.* warning: / => sub {
		my ($msg) = @_;
		print STDERR "\e[33;1m$msg\e[0m\n";
	} ],

	[ qr/^.* error: / => sub {
		my ($msg) = @_;
		print STDERR "An error occured when executing:\e[37;1m $cc " . join(' ', @ARGV) . "\n";
		print STDERR "\e[31;1m$msg\e[0m\n";
	} ],
);

my $pid;

my ($r_stderr, $w_stderr);

my $name = "";

foreach my $n (@ARGV)
{
	if ($n =~ /\.cpp$/)
	{
		$name = $n;
	}
}

if (!defined($cc) || $cc eq "") {
	die "Compiler not specified!\n";
}

pipe($r_stderr, $w_stderr) or die "pipe stderr: $!\n";

$pid = fork;

die "Cannot fork to start gcc! $!\n" unless defined($pid);

if ($pid) {

	print "\t\e[1;32mBUILD:\e[0m\t\t$name\n" unless $name eq "";

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
