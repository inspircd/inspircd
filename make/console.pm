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

use Class::Struct         qw(struct);
use Exporter              qw(import);
use File::Path            qw(mkpath);
use File::Spec::Functions qw(rel2abs);

our @EXPORT = qw(command
                 execute_command
                 console_format
                 print_error
                 print_warning
                 prompt_bool
                 prompt_dir
                 prompt_string);

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

my %commands;

struct 'command' => {
	'callback'    => '$',
	'description' => '$',
};

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

sub print_warning {
	print STDERR console_format "<|YELLOW Warning:|> ";
	for my $line (@_) {
		say STDERR console_format $line;
	}
}

sub prompt_bool($$$) {
	my ($interactive, $question, $default) = @_;
	while (1) {
		my $answer = prompt_string($interactive, $question, $default ? 'yes' : 'no');
		return 1 if $answer =~ /^y(?:es)?$/i;
		return 0 if $answer =~ /^no?$/i;
		print_warning "\"$answer\" is not \"yes\" or \"no\". Please try again.\n";
	}
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
	say console_format $question;
	print console_format "[<|GREEN $default|>] => ";
	chomp(my $answer = <STDIN>);
	say '';
	return $answer ? $answer : $default;
}

sub command($$$) {
	my ($name, $description, $callback) = @_;
	$commands{$name} = command->new;
	$commands{$name}->callback($callback);
	$commands{$name}->description($description);
}

sub command_alias($$) {
	my ($source, $target) = @_;
	command $source, undef, sub(@) {
		execute_command $target, @_;
	};
}

sub execute_command(@) {
	my $command = defined $_[0] ? lc shift : 'help';
	if ($command eq 'help') {
		say console_format "<|GREEN Usage:|> $0 <<|UNDERLINE COMMAND|>> [<|UNDERLINE OPTIONS...|>]";
		say '';
		say console_format "<|GREEN Commands:|>";
		for my $key (sort keys %commands) {
			next unless defined $commands{$key}->description;
			my $name = sprintf "%-15s", $key;
			my $description = $commands{$key}->description;
			say console_format "  <|BOLD $name|> # $description";
		}
		exit 0;
	} elsif (!$commands{$command}) {
		print_error "no command called <|BOLD $command|> exists!",
			"See <|BOLD $0 help|> for a list of commands.";
	} else {
		return $commands{$command}->callback->(@_);
	}
}

1;
