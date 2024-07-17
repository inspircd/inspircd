#!/usr/bin/env perl
#
# InspIRCd -- Internet Relay Chat Daemon
#
#   Copyright (C) 2019 iwalkalone <iwalkalone69@gmail.com>
#   Copyright (C) 2014 Attila Molnar <attilamolnar@hush.com>
#   Copyright (C) 2013, 2015-2016, 2018, 2021 Sadie Powell <sadie@witchery.services>
#   Copyright (C) 2012 Robby <robby@chatbelgie.be>
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
# TODO: kill this brittle mess before v5.


use v5.26.0;
use strict;
use warnings FATAL => qw(all);

use File::Basename        qw(dirname);
use File::Spec::Functions qw(abs2rel);
use FindBin               qw($RealDir);

use lib dirname $RealDir;
use make::console;
use make::directive;

chdir $ENV{BUILDPATH};

my $type = shift;
my $out = shift;

if ($type eq 'core-ld') {
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

sub message($$$) {
	my ($type, $file, $command) = @_;
	if ($ENV{INSPIRCD_VERBOSE}) {
		say $command;
	} else {
		say console_format "\t<|GREEN $type:|>\t\t$file";
	}
}

sub rpath($) {
	my $message = shift;
	$message =~ s/-L(\S+)/-Wl,-rpath,$1 -L$1/g unless defined $ENV{INSPIRCD_DISABLE_RPATH};
	return $message;
}

sub do_core_link {
	my $execstr = "$ENV{CXX} -o $out $ENV{CORELDFLAGS} @_ $ENV{LDLIBS}";
	message 'LINK', $out, $execstr;
	exec $execstr;
}

sub do_link_dir {
	my ($dir, $link_flags) = (shift, '');
	for my $file (<$dir/*.cpp>) {
		$link_flags .= rpath(get_directive($file, 'LinkerFlags', '')) . ' ';
	}
	my $execstr = "$ENV{CXX} -o $out $ENV{PICLDFLAGS} @_ $link_flags";
	message 'LINK', $out, $execstr;
	exec $execstr;
}

sub do_compile {
	my ($do_compile, $do_link, $file) = @_;

	my $flags = '';
	my $libs = '';
	if ($do_compile) {
		$flags = $ENV{CORECXXFLAGS} . ' ' . get_directive($file, 'CompilerFlags', '');

		if ($file =~ m#/modules/(?:core/)?([a-z0-9_]+)#) {
			my $name = $1;
			my $prefix = $1 =~ '^core_' ? '' : 'm_';
			$flags .= ' -DMODNAME=\\"'.$prefix.$name.'\\"';
		}
	}

	if ($do_link) {
		$flags = join ' ', $flags, $ENV{PICLDFLAGS};
		$libs = rpath(get_directive($file, 'LinkerFlags', ''));
	} else {
		$flags .= ' -c';
	}

	my $execstr = "$ENV{CXX} -o $out $flags $file $libs";
	message 'BUILD', abs2rel($file =~ s#/src/\.\./#/#gr, $ENV{SOURCEPATH}), $execstr;
	exec $execstr;
}
