#
# InspIRCd -- Internet Relay Chat Daemon
#
#   Copyright (C) 2014-2017, 2019-2022 Sadie Powell <sadie@witchery.services>
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


package make::console;

use v5.26.0;
use strict;
use warnings FATAL => qw(all);

use Exporter              qw(import);

our @EXPORT = qw(
	console_format
	print_error
);

my %FORMAT_CODES = (
	DEFAULT   => "\e[0m",
	BOLD      => "\e[1m",
	ITALIC    => "\e[3m",
	UNDERLINE => "\e[4m",

	RED    => "\e[1;31m",
	GREEN  => "\e[1;32m",
	YELLOW => "\e[1;33m",
	BLUE   => "\e[1;34m"
);

sub console_format($) {
	my $message = shift;
	while ($message =~ /(<\|(\S+)\s(.*?)\|>)/) {
		my ($match, $type, $text) = ($1, uc $2, $3);
		if (-t STDOUT && exists $FORMAT_CODES{$type}) {
			$message =~ s/\Q$match\E/$FORMAT_CODES{$type}$text$FORMAT_CODES{DEFAULT}/;
		} else {
			$message =~ s/\Q$match\E/$text/;
		}
	}
	return $message;
}

sub print_error {
	print STDERR console_format "<|RED Error:|> ";
	for my $line (@_) {
		say STDERR console_format $line;
	}
	exit 1;
}

1;
