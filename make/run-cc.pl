#!/usr/bin/env perl

#
# InspIRCd -- Internet Relay Chat Daemon
#
#   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
#   Copyright (C) 2008 Craig Edwards <craigedwards@brainbox.cc>
#
# This file is part of InspIRCd.  InspIRCd is free software: you can
# redistribute it and/or modify it under the terms of the GNU General Public
# License as published by the Free Software Foundation, version 2.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
# details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#


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
		my $errstr = $location . "$where error: cannot pass objects of non-POD type `$type' through `...'\n";
		$location = "";
		if ($type =~ m/::(basic_)?string/) {
			$errstr .= "$where (Did you forget to call c_str()?)\n";
		}
		die $errstr;
	} ],

	# Start of an include stack.
	[ qr/^In file included from .*[,:]$/ => sub {
		my ($msg) = @_;
		$location = "$msg\n";
		return undef;
	} ],

	# Continuation of an include stack.
	[ qr/^                 from .*[,:]$/ => sub {
		my ($msg) = @_;
		$location .= "$msg\n";
		return undef;
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
		return undef;
	} ],

	[ qr/^.* warning: / => sub {
		my ($msg) = @_;
		my $str = $location . "\e[33;1m$msg\e[0m\n";
		$showncmdline = 1;
		$location = "";
		return $str;
	} ],

	[ qr/^.* error: / => sub {
		my ($msg) = @_;
		my $str = "";
		$str = "An error occured when executing:\e[37;1m $cc " . join(' ', @ARGV) . "\n" unless $showncmdline;
		$showncmdline = 1;
		$str .= $location . "\e[31;1m$msg\e[0m\n";
		$location = "";
		return $str;
	} ],

	[ qr/./ => sub {
		my ($msg) = @_;
		$msg = $location . $msg;
		$location = "";
		$msg =~ s/std::basic_string\<char\, std\:\:char_traits\<char\>, std::allocator\<char\> \>(\s+|)/std::string/g;
		$msg =~ s/std::basic_string\<char\, .*?irc_char_traits\<char\>, std::allocator\<char\> \>(\s+|)/irc::string/g;
		for my $stl (qw(deque vector list)) {
			$msg =~ s/std::$stl\<(\S+), std::allocator\<\1\> \>/std::$stl\<$1\>/g;
			$msg =~ s/std::$stl\<(std::pair\<\S+, \S+\>), std::allocator\<\1 \> \>/std::$stl<$1 >/g;
		}
		$msg =~ s/std::map\<(\S+), (\S+), std::less\<\1\>, std::allocator\<std::pair\<const \1, \2\> \> \>/std::map<$1, $2>/g;
		# Warning: These filters are GNU C++ specific!
		$msg =~ s/__gnu_cxx::__normal_iterator\<(\S+)\*, std::vector\<\1\> \>/std::vector<$1>::iterator/g;
		$msg =~ s/__gnu_cxx::__normal_iterator\<(std::pair\<\S+, \S+\>)\*, std::vector\<\1 \> \>/std::vector<$1 >::iterator/g;
		$msg =~ s/__gnu_cxx::__normal_iterator\<char\*, std::string\>/std::string::iterator/g;
		$msg =~ s/__gnu_cxx::__normal_iterator\<char\*, irc::string\>/irc::string::iterator/g;
		return $msg;
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
			my $f = $n;
			if (defined $ENV{SOURCEPATH}) {
				$f =~ s#^$ENV{SOURCEPATH}/src/##;
			}
			if ($action eq "BUILD")
			{
				$name .= " " . $f;
			}
			else
			{
				$action = "BUILD";
				$name = $f;
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
				$line = eval {
					$filter->[1]->($line, @caps);
				};
				if ($@) {
					# Note that $line is undef now.
					$fail = 1;
					print STDERR $@;
				}
				next LINE unless defined($line);
			}
		}
		# Chomp off newlines again, in case the filters put some back in.
		chomp $line;
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
