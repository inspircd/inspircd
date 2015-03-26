#
# InspIRCd -- Internet Relay Chat Daemon
#
#   Copyright (C) 2013-2014 Peter Powell <petpow@saberuk.com>
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


BEGIN {
	require 5.10.0;
}

package make::common;

use feature ':5.10';
use strict;
use warnings FATAL => qw(all);

use Exporter              qw(import);
use File::Spec::Functions qw(rel2abs);

our @EXPORT = qw(get_cpu_count
                 get_version
                 module_installed);

sub get_version {
	state %version;
	return %version if %version;

	# Attempt to retrieve version information from src/version.sh
	chomp(my $vf = `sh src/version.sh 2>/dev/null`);
	if ($vf =~ /^InspIRCd-([0-9]+)\.([0-9]+)\.([0-9]+)(?:\+(\w+))?$/) {
		%version = ( MAJOR => $1, MINOR => $2, PATCH => $3, LABEL => $4 );
	}

	# Attempt to retrieve missing version information from Git
	chomp(my $gr = `git describe --tags 2>/dev/null`);
	if ($gr =~ /^v([0-9]+)\.([0-9]+)\.([0-9]+)(?:-\d+-g(\w+))?$/) {
		$version{MAJOR} //= $1;
		$version{MINOR} //= $2;
		$version{PATCH} //= $3;
		$version{LABEL} = $4 if defined $4;
	}

	# The user is using a stable release which does not have
	# a label attached.
	$version{LABEL} //= 'release';

	# If any of these fields are missing then the user has deleted the
	# version file and is not running from Git. Fill in the fields with
	# dummy data so we don't get into trouble with undef values later.
	$version{MAJOR} //= '0';
	$version{MINOR} //= '0';
	$version{PATCH} //= '0';

	return %version;
}

sub module_installed($) {
	my $module = shift;
	eval("use $module;");
	return !$@;
}

sub get_cpu_count {
	my $count = 1;
	if ($^O =~ /bsd/) {
		$count = `sysctl -n hw.ncpu`;
	} elsif ($^O eq 'darwin') {
		$count = `sysctl -n hw.activecpu`;
	} elsif ($^O eq 'linux') {
		$count = `getconf _NPROCESSORS_ONLN`;
	} elsif ($^O eq 'solaris') {
		$count = `psrinfo -p`;
	}
	chomp($count);
	return $count;
}

1;
