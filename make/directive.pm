#
# InspIRCd -- Internet Relay Chat Daemon
#
#   Copyright (C) 2016-2025 Sadie Powell <sadie@sadiepowell.dev>
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
use make::console;

use constant DIRECTIVE_ERROR_PIPE => $ENV{INSPIRCD_VERBOSE} ? '' : '2>/dev/null';

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

1;
