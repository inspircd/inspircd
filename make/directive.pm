#
# InspIRCd -- Internet Relay Chat Daemon
#
#   Copyright (C) 2016 Peter Powell <petpow@saberuk.com>
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


package make::directive;

BEGIN {
	require 5.10.0;
}

use feature ':5.10';
use strict;
use warnings FATAL => qw(all);

use File::Basename qw(basename);
use Exporter       qw(import);

use make::configure;
use make::console;

use constant DIRECTIVE_ERROR_PIPE => $ENV{INSPIRCD_VERBOSE} ? '' : '2>/dev/null';

our @EXPORT = qw(get_directive
                 execute_functions);

sub get_directive($$;$)
{
	my ($file, $property, $default) = @_;
	open(MODULE, $file) or return $default;

	my $value = '';
	while (<MODULE>) {
		if ($_ =~ /^\/\* \$(\S+): (.+) \*\/$/ || $_ =~ /^\/\/\/ \$(\S+): (.+)/) {
			next unless $1 eq $property;
			$value .= ' ' . execute_functions($file, $1, $2);
		}
	}
	close(MODULE);

	# Strip all extraneous whitespace.
	$value =~ s/^\s+|\s+$//g;
	return $value || $default;
}

sub execute_functions($$$) {
	my ($file, $name, $line) = @_;

	# NOTE: we have to use 'our' instead of 'my' here because of a Perl bug.
	for (our @parameters = (); $line =~ /([a-z_]+)\((?:\s*"([^"]*)(?{push @parameters, $2})"\s*)*\)/; undef @parameters) {
		my $sub = make::directive->can("__function_$1");
		print_error "unknown $name directive '$1' in $file!" unless $sub;

		# Call the subroutine and replace the function.
		my $result = $sub->($file, @parameters);
		if (defined $result) {
			$line = $` . $result . $';
			next;
		}

		# If the subroutine returns undef then it is a sign that we should
		# disregard the rest of the line and stop processing it.
		$line = $`;
	}

	return $line;
}

sub __environment {
	my ($prefix, $suffix) = @_;
	$suffix =~ s/[-.]/_/g;
	$suffix =~ s/[^A-Za-z0-9_]//g;
	return $prefix . uc $suffix;
}

sub __error {
	my ($file, @message) = @_;
	push @message, '';

	# If we have package details then suggest to the user that they check
	# that they have the packages installed.=
	my $dependencies = get_directive($file, 'PackageInfo');
	if (defined $dependencies) {
		my @packages = sort grep { /^\S+$/ } split /\s/, $dependencies;
		push @message, 'You should make sure you have the following packages installed:';
		for (@packages) {
			push @message, " * $_";
		}
	} else {
		push @message, 'You should make sure that you have all of the required dependencies';
		push @message, 'for this module installed.';
	}
	push @message, '';

	# If we have author information then tell the user to report the bug
	# to them. Otherwise, assume it is a bundled module and tell the user
	# to report it to the InspIRCd issue tracker.
	my $author = get_directive($file, 'ModAuthor');
	if (defined $author) {
		push @message, 'If you believe this error to be a bug then you can try to contact the';
		push @message, 'author of this module:';
		my $author_mail = get_directive($file, 'ModAuthorMail');
		if (defined $author_mail) {
			push @message, " * $author <$author_mail>";
		} else {
			push @message, " * $author";
		}
	} else {
		push @message, 'If you believe this error to be a bug then you can file a bug report';
		push @message, 'at https://github.com/inspircd/inspircd/issues';
	}
	push @message, '';

	push @message, 'If you would like help with fixing this problem then visit our IRC';
	push @message, 'channel at irc.inspircd.org #InspIRCd for support.';
	push @message, '';

	print_error @message;
}

sub __function_error {
	my ($file, @messages) = @_;
	__error $file, @messages;
}

sub __function_execute {
	my ($file, $command, $environment, $defaults) = @_;

	# Try to execute the command...
	chomp(my $result = `$command ${\DIRECTIVE_ERROR_PIPE}`);
	unless ($?) {
		print_format "Execution of `<|GREEN $command|>` succeeded: <|BOLD $result|>\n";
		return $result;
	}

	# If looking up with pkg-config fails then check the environment...
	if (defined $environment && $environment ne '') {
		$environment = __environment 'INSPIRCD_', $environment;
		if (defined $ENV{$environment}) {
			print_format "Execution of `<|GREEN $command|>` failed; using the environment: <|BOLD $ENV{$environment}|>\n";
			return $ENV{$environment};
		}
	}

	# If all else fails then look for the defaults..
	if (defined $defaults) {
		print_format "Execution of `<|GREEN $command|>` failed; using the defaults: <|BOLD $defaults|>\n";
		return $defaults;
	}

	# Executing the command failed and we don't have any defaults so give up. 
	__error $file, "`<|GREEN $command|>` exited with a non-zero exit code!";
}

sub __function_find_compiler_flags {
	my ($file, $name, $defaults) = @_;

	# Try to look up the compiler flags with pkg-config...
	chomp(my $flags = `pkg-config --cflags $name ${\DIRECTIVE_ERROR_PIPE}`);
	unless ($?) {
		print_format "Found the compiler flags for <|GREEN ${\basename $file, '.cpp'}|> using pkg-config: <|BOLD $flags|>\n";
		return $flags;
	}

	# If looking up with pkg-config fails then check the environment...
	my $key = __environment 'INSPIRCD_CXXFLAGS_', $name;
	if (defined $ENV{$key}) {
		print_format "Found the compiler flags for <|GREEN ${\basename $file, '.cpp'}|> using the environment: <|BOLD $ENV{$key}|>\n";
		return $ENV{$key};
	}

	# If all else fails then look for the defaults..
	if (defined $defaults) {
		print_format "Found the compiler flags for <|GREEN ${\basename $file, '.cpp'}|> using the defaults: <|BOLD $defaults|>\n";
		return $defaults;
	}

	# We can't find it via pkg-config, via the environment, or via the defaults so give up.
	__error $file, "unable to find the compiler flags for <|GREEN ${\basename $file, '.cpp'}|>!";
}

sub __function_find_linker_flags {
	my ($file, $name, $defaults) = @_;

	# Try to look up the linker flags with pkg-config...
	chomp(my $flags = `pkg-config --libs $name ${\DIRECTIVE_ERROR_PIPE}`);
	unless ($?) {
		print_format "Found the linker flags for <|GREEN ${\basename $file, '.cpp'}|> using pkg-config: <|BOLD $flags|>\n";
		return $flags;
	}

	# If looking up with pkg-config fails then check the environment...
	my $key = __environment 'INSPIRCD_CXXFLAGS_', $name;
	if (defined $ENV{$key}) {
		print_format "Found the linker flags for <|GREEN ${\basename $file, '.cpp'}|> using the environment: <|BOLD $ENV{$key}|>\n";
		return $ENV{$key};
	}

	# If all else fails then look for the defaults..
	if (defined $defaults) {
		print_format "Found the linker flags for <|GREEN ${\basename $file, '.cpp'}|> using the defaults: <|BOLD $defaults|>\n";
		return $defaults;
	}

	# We can't find it via pkg-config, via the environment, or via the defaults so give up.
	__error $file, "unable to find the linker flags for <|GREEN ${\basename $file, '.cpp'}|>!";
}

sub __function_require_system {
	my ($file, $name, $minimum, $maximum) = @_;
	my ($system, $version);

	# Linux is special and can be compared by distribution names.
	if ($^O eq 'linux' && $name ne 'linux') {
		chomp($system = lc `lsb_release --id --short 2>/dev/null`);
		chomp($version = lc `lsb_release --release --short 2>/dev/null`);
	}

	# Gather information on the system if we don't have it already.
	chomp($system ||= lc `uname -s 2>/dev/null`);
	chomp($version ||= lc `uname -r 2>/dev/null`);

	# We only care about the important bit of the version number so trim the rest.
	$version =~ s/^(\d+\.\d+).+/$1/;

	# Check whether the current system is suitable.
	return undef if $name ne $system;
	return undef if defined $minimum && $version < $minimum;
	return undef if defined $maximum && $version > $maximum;

	# Requirement directives don't change anything directly.
	return "";
}

sub __function_require_version {
	my ($file, $name, $minimum, $maximum) = @_;

	# If pkg-config isn't installed then we can't do anything here.
	if (system "pkg-config --exists $name ${\DIRECTIVE_ERROR_PIPE}") {
		print_warning "unable to look up the version of $name using pkg-config!";
		return undef;
	}

	# Check with pkg-config whether we have the required version.
	return undef if defined $minimum && system "pkg-config --atleast-version $minimum $name";
	return undef if defined $maximum && system "pkg-config --max-version $maximum $name";

	# Requirement directives don't change anything directly.
	return "";
}

sub __function_warning {
	my ($file, @messages) = @_;
	print_warning @messages;
}

1;
