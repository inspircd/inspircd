#!/usr/bin/env perl

#
# InspIRCd -- Internet Relay Chat Daemon
#
#   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
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


use strict;
use warnings;
BEGIN { push @INC, $ENV{SOURCEPATH}; }
use make::configure;

chdir $ENV{BUILDPATH};

my $type = shift;
my $out = shift;
my $verbose = ($type =~ s/-v$//);

## BEGIN HACK: REMOVE IN 2.2!
sub read_config_cache {
	my %cfg = ();
	open(CACHE, '../.config.cache') or return %cfg;
	while (my $line = <CACHE>) {
		next if $line =~ /^\s*($|\#)/;
		my ($key, $value) = ($line =~ /^(\S+)="(.*)"$/);
		$cfg{$key} = $value;
	}
	close(CACHE);
	return %cfg;
}

our %config = read_config_cache();
## END HACK

if ($type eq 'gen-ld') {
	do_static_find(@ARGV);
} elsif ($type eq 'static-ld') {
	do_static_link(@ARGV);
} elsif ($type eq 'core-ld') {
	do_core_link(@ARGV);
} elsif ($type eq 'link-dir') {
	do_link_dir(@ARGV);
} elsif ($type eq 'gen-o') {
	do_compile(1, 0, @ARGV);
} elsif ($type eq 'gen-so') {
	do_compile(1, 1, @ARGV);
} elsif ($type eq 'link-so') {
	do_compile(0, 1, @ARGV);
} else {
	print STDERR "Unknown unit-cc subcommand $type!\n";
}
exit 1;

sub do_static_find {
	my @flags;
	for my $file (@ARGV) {
		push @flags, getlinkerflags($file);
	}
	open F, '>', $out;
	print F join ' ', @flags;
	close F;
	exit 0;
}

sub do_static_link {
	my $execstr = "$ENV{RUNLD} -o $out $ENV{CORELDFLAGS}";
	for (@ARGV) {
		if (/\.cmd$/) {
			open F, '<', $_;
			my $libs = <F>;
			chomp $libs;
			$execstr .= ' '.$libs;
			close F;
		} else {
			$execstr .= ' '.$_;
		}
	}
	$execstr .= ' '.$ENV{LDLIBS};
	print "$execstr\n" if $verbose;
	exec $execstr;
}

sub do_core_link {
	my $execstr = "$ENV{RUNLD} -o $out $ENV{CORELDFLAGS} @_ $ENV{LDLIBS}";
	print "$execstr\n" if $verbose;
	exec $execstr;
}

sub do_link_dir {
	my $execstr = "$ENV{RUNLD} -o $out $ENV{PICLDFLAGS} @_";
	print "$execstr\n" if $verbose;
	exec $execstr;
}

sub do_compile {
	my ($do_compile, $do_link, $file) = @_;

	my $flags = '';
	my $libs = '';
	my $binary = $ENV{RUNCC};
	if ($do_compile) {
		$flags = $ENV{CXXFLAGS};
		$flags =~ s/ -pedantic// if nopedantic($file);
		$flags .= ' ' . getcompilerflags($file);

		if ($file =~ m#(?:^|/)((?:m|cmd)_[^/. ]+)(?:\.cpp|/.*\.cpp)$#) {
			$flags .= ' -DMODNAME='.$1.'.so';
		}
	} else {
		$binary = $ENV{RUNLD};
	}

	if ($do_link) {
		$flags = join ' ', $flags, $ENV{PICLDFLAGS};
		$libs = join ' ', getlinkerflags($file);
	} else {
		$flags .= ' -c';
	}

	my $execstr = "$binary -o $out $flags $file $libs";
	print "$execstr\n" if $verbose;
	exec $execstr;
}
