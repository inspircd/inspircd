#
# InspIRCd -- Internet Relay Chat Daemon
#
#   Copyright (C) 2016-2024 Sadie Powell <sadie@witchery.services>
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

use v5.26.0;
use strict;
use warnings FATAL => qw(all);

use File::Basename        qw(dirname);
use File::Spec::Functions qw(catdir);
use Exporter              qw(import);

use make::common;
use make::configure;
use make::console;

use constant DIRECTIVE_ERROR_PIPE => $ENV{INSPIRCD_VERBOSE} ? '' : '2>/dev/null';
use constant PKG_CONFIG           => $ENV{PKG_CONFIG} || 'pkg-config';

our @EXPORT = qw(
	get_directives
	get_directive
	execute_functions
);

sub get_directives($$;$) {
	my ($file, $property, $functions) = @_;
	$functions //= 1;
	open(my $fh, $file) or return ();

	my @values;
	while (<$fh>) {
		if ($_ =~ /^\/\* \$(\S+): (.+) \*\/$/ || $_ =~ /^\/\/\/ \$(\S+): (.+)/) {
			next unless $1 eq $property;
			my $value = $functions ? execute_functions($file, $1, $2) : $2;
			push @values, $value;
		}
	}
	close $fh;
	return @values;
}

sub get_directive($$;$$) {
	my ($file, $property, $default, $functions) = @_;
	my @values = get_directives($file, $property, $functions);

	my $value = join ' ', @values;
	$value =~ s/^\s+|\s+$//g;
	return $value || $default;
}

sub execute_functions($$$) {
	my ($file, $name, $line) = @_;

	# NOTE: we have to use 'our' instead of 'my' here because of a Perl bug.
	for (our @parameters = (); $line =~ /([a-z_]+)\((?:\s*"([^"]*)(?{push @parameters, $2})"\s*)*\)/; undef @parameters) {
		my $sub = make::directive->can("__function_$1");
		print_error "unknown $name function '$1' in $file!" unless $sub;

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

sub __clean_flags($) {
	my $flags = shift;

	# It causes problems if a dependency tries to force a specific C++ version
	# so we strip it and handle it ourselves.
	$flags =~ s/(?:^|\s+)-std=(?:c|gnu)\+\+[A-Za-z0-9]+(?:\$|\s+)/ /g;

	# Strip any whitespace our changes have left behind.
	$flags =~ s/^\s+|\s+$//g;

	return $flags;
}

sub __environment {
	my ($prefix, $suffix) = @_;
	$suffix =~ s/[-.]/_/g;
	$suffix =~ s/[^A-Za-z0-9_]//g;
	return $prefix . uc $suffix;
}

sub __env_or_unset {
	my $name = shift;
	return $ENV{$name} || '<|ITALIC unset|>';
};

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
	my @authors = get_directives($file, 'ModAuthor');
	if (@authors) {
		push @message, 'If you believe this error to be a bug then you can try to contact the';
		push @message, "author${$#authors ? \'s' : \''} of this module:";
		for my $author (@authors) {
			push @message, " * $author";
		}
	} else {
		my %version = get_version();
		push @message, 'If you believe this error to be a bug then you can file a bug report';
		push @message, 'at https://github.com/inspircd/inspircd/issues. Please include the';
		push @message, 'following data in your report:';
		push @message, " * CPPFLAGS:        ${\__env_or_unset 'CPPFLAGS'}";
		push @message, " * CXXFLAGS:        ${\__env_or_unset 'CXXFLAGS'}";
		push @message, " * LDFLAGS:         ${\__env_or_unset 'LDFLAGS'}";
		push @message, " * PATH:            ${\__env_or_unset 'PATH'}";
		push @message, " * PKG_CONFIG:      ${\__env_or_unset 'PKG_CONFIG'}";
		push @message, " * PKG_CONFIG_PATH: ${\__env_or_unset 'PKG_CONFIG_PATH'}";
		push @message, '';
		push @message, 'You can also refer to the documentation page for this module at';
		push @message, "https://docs.inspircd.org/$version{MAJOR}/modules/${\module_shrink $file}.";
	}
	push @message, '';

	push @message, 'If you would like help with fixing this problem then visit our IRC';
	push @message, 'channel at ircs://irc.teranova.net/inspircd or create a support';
	push @message, 'discussion at https://github.com/inspircd/inspircd/discussions.';
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
		$result = __clean_flags $result;
		say console_format "Execution of `<|GREEN $command|>` succeeded: <|BOLD $result|>";
		return $result;
	}

	# If looking up with pkg-config fails then check the environment...
	if (defined $environment && $environment ne '') {
		$environment = __environment 'INSPIRCD_', $environment;
		if (defined $ENV{$environment}) {
			say console_format "Execution of `<|GREEN $command|>` failed; using the environment: <|BOLD $ENV{$environment}|>";
			return $ENV{$environment};
		}
	}

	# If all else fails then look for the defaults..
	if (defined $defaults) {
		say console_format "Execution of `<|GREEN $command|>` failed; using the defaults: <|BOLD $defaults|>";
		return $defaults;
	}

	# Executing the command failed and we don't have any defaults so give up.
	__error $file, "`<|GREEN $command|>` exited with a non-zero exit code!";
}

sub __function_find_compiler_flags {
	my ($file, $name, $defaults) = @_;

	# Try to look up the compiler flags with pkg-config...
	chomp(my $flags = `${\PKG_CONFIG} --cflags $name ${\DIRECTIVE_ERROR_PIPE}`);
	unless ($?) {
		$flags = __clean_flags $flags;
		say console_format "Found the <|GREEN $name|> compiler flags for <|GREEN ${\module_shrink $file}|> using pkg-config: <|BOLD $flags|>";
		return $flags;
	}

	# If looking up with pkg-config fails then check the environment...
	my $key = __environment 'INSPIRCD_CXXFLAGS_', $name;
	if (defined $ENV{$key}) {
		say console_format "Found the <|GREEN $name|> compiler flags for <|GREEN ${\module_shrink $file}|> using the environment: <|BOLD $ENV{$key}|>";
		return $ENV{$key};
	}

	# If all else fails then look for the defaults..
	if (defined $defaults) {
		say console_format "Using the default <|GREEN $name|> compiler flags for <|GREEN ${\module_shrink $file}|>: <|BOLD $defaults|>";
		return $defaults;
	}

	# We can't find it via pkg-config, via the environment, or via the defaults so give up.
	__error $file, "unable to find the <|GREEN $name|> compiler flags for <|GREEN ${\module_shrink $file}|>!";
}

sub __function_find_linker_flags {
	my ($file, $name, $defaults) = @_;

	# Try to look up the linker flags with pkg-config...
	chomp(my $flags = `${\PKG_CONFIG} --libs $name ${\DIRECTIVE_ERROR_PIPE}`);
	unless ($?) {
		$flags = __clean_flags $flags;
		say console_format "Found the <|GREEN $name|> linker flags for <|GREEN ${\module_shrink $file}|> using pkg-config: <|BOLD $flags|>";
		return $flags;
	}

	# If looking up with pkg-config fails then check the environment...
	my $key = __environment 'INSPIRCD_CXXFLAGS_', $name;
	if (defined $ENV{$key}) {
		say console_format "Found the <|GREEN $name|> linker flags for <|GREEN ${\module_shrink $file}|> using the environment: <|BOLD $ENV{$key}|>";
		return $ENV{$key};
	}

	# If all else fails then look for the defaults..
	if (defined $defaults) {
		say console_format "Using the default <|GREEN $name|> linker flags for <|GREEN ${\module_shrink $file}|>: <|BOLD $defaults|>";
		return $defaults;
	}

	# We can't find it via pkg-config, via the environment, or via the defaults so give up.
	__error $file, "unable to find the <|GREEN $name|> linker flags for <|GREEN ${\module_shrink $file}|>!";
}

sub __function_require_compiler {
	my ($file, $name, $minimum, $maximum) =  @_;

	# Check for an inverted match.
	my ($ok, $err) = ("", undef);
	if ($name =~ s/^!//) {
		($ok, $err) = ($err, $ok);
	}

	# Look up information about the compiler.
	return $err unless $ENV{CXX};
	my %compiler = get_compiler_info($ENV{CXX});

	# Check whether the current compiler is suitable.
	return $err unless $compiler{NAME} eq $name;
	return $err if defined $minimum && $compiler{VERSION} < $minimum;
	return $err if defined $maximum && $compiler{VERSION} > $maximum;

	# Requirement directives don't change anything directly.
	return $ok;
}

sub __function_require_system {
	my ($file, $name, $minimum, $maximum) = @_;
	my ($system, $system_like, $version);

	# Check for an inverted match.
	my ($ok, $err) = ("", undef);
	if ($name =~ s/^!//) {
		($ok, $err) = ($err, $ok);
	}

	# If a system name ends in a tilde we match on alternate names.
	my $match_like = $name =~ s/~$//;

	# Linux is special and can be compared by distribution names.
	if ($^O eq 'linux' && $name ne 'linux') {
		chomp($system      = lc `sh -c '. /etc/os-release 2>/dev/null && echo \$ID'`);
		chomp($system_like = lc `sh -c '. /etc/os-release 2>/dev/null && echo \$ID_LIKE'`);
		chomp($version     = lc `sh -c '. /etc/os-release 2>/dev/null && echo \$VERSION_ID'`);
	}

	# Gather information on the system if we don't have it already.
	chomp($system ||= lc `uname -s 2>/dev/null`);
	chomp($version ||= lc `uname -r 2>/dev/null`);
	$system_like ||= '';

	# We only care about the important bit of the version number so trim the rest.
	$version =~ s/^(\d+\.\d+).+/$1/;

	# Check whether the current system is suitable.
	if ($name ne $system) {
		return $err unless $match_like;
		return $err unless grep { $_ eq $name } split /\s+/, $system_like;
	}
	return $err if defined $minimum && $version < $minimum;
	return $err if defined $maximum && $version > $maximum;

	# Requirement directives don't change anything directly.
	return $ok;
}

sub __function_require_library {
	my ($file, $name, $minimum, $maximum) = @_;

	# Check for an inverted match.
	my ($ok, $err) = ("", undef);
	if ($name =~ s/^!//) {
		($ok, $err) = ($err, $ok);
	}

	# If pkg-config isn't installed then we can't do anything here.
	if (system "${\PKG_CONFIG} --version 1>/dev/null 2>/dev/null") {
		print_warning "unable to look up the version of <|GREEN $name|> using pkg-config!";
		return $err;
	}

	# Check with pkg-config whether we have the required version.
	return $err if system "${\PKG_CONFIG} --exists $name";
	return $err if defined $minimum && system "${\PKG_CONFIG} --atleast-version $minimum $name";
	return $err if defined $maximum && system "${\PKG_CONFIG} --max-version $maximum $name";

	# Requirement directives don't change anything directly.
	return $ok;
}

sub __function_warning {
	my ($file, @messages) = @_;
	print_warning @messages;
}

1;
