#
# InspIRCd -- Internet Relay Chat Daemon
#
#   Copyright (C) 2014 Peter Powell <petpow@saberuk.com>
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

BEGIN {
	require 5.10.0;
}

use feature ':5.10';
use strict;
use warnings FATAL => qw(all);

use File::Path            qw(mkpath);
use File::Spec::Functions qw(rel2abs);
use Exporter              qw(import);

our @EXPORT = qw(print_format
                 print_error
                 print_warning
                 prompt_bool
                 prompt_dir
                 prompt_string);

my %FORMAT_CODES = (
	DEFAULT => "\e[0m",
	BOLD    => "\e[1m",

	RED    => "\e[1;31m",
	GREEN  => "\e[1;32m",
	YELLOW => "\e[1;33m",
	BLUE   => "\e[1;34m"
);

sub __console_format($$) {
	my ($name, $data) = @_;
	return $data unless -t STDOUT;
	return $FORMAT_CODES{uc $name} . $data . $FORMAT_CODES{DEFAULT};
}

sub print_format($;$) {
	my $message = shift;
	my $stream = shift // *STDOUT;
	while ($message =~ /(<\|(\S+)\s(.*?)\|>)/) {
		my $formatted = __console_format $2, $3;
		$message =~ s/\Q$1\E/$formatted/;
	}
	print { $stream } $message;
}

sub print_error {
	print_format "<|RED Error:|> ", *STDERR;
	for my $line (@_) {
		print_format "$line\n", *STDERR;
	}
	exit 1;
}

sub print_warning {
	print_format "<|YELLOW Warning:|> ", *STDERR;
	for my $line (@_) {
		print_format "$line\n", *STDERR;
	}
}

sub prompt_bool($$$) {
	my ($interactive, $question, $default) = @_;
	my $answer = prompt_string($interactive, $question, $default ? 'y' : 'n');
	return $answer =~ /y/i;
}

sub prompt_dir($$$;$) {
	my ($interactive, $question, $default, $create_now) = @_;
	my ($answer, $create);
	do {
		$answer = rel2abs(prompt_string($interactive, $question, $default));
		$create = prompt_bool($interactive && !-d $answer, "$answer does not exist. Create it?", 'y');
		if ($create && $create_now) {
			unless (create_directory $answer, 0750) {
				print_warning "unable to create $answer: $!\n";
				$create = 0;
			}
		}
	} while (!$create);
	return $answer;
}

sub prompt_string($$$) {
	my ($interactive, $question, $default) = @_;
	return $default unless $interactive;
	print_format "$question\n";
	print_format "[<|GREEN $default|>] => ";
	chomp(my $answer = <STDIN>);
	say '';
	return $answer ? $answer : $default;
}

1;
