#
# InspIRCd -- Internet Relay Chat Daemon
#
#   Copyright (C) 2014-2017, 2019-2021, 2024 Sadie Powell <sadie@witchery.services>
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


package make::common;

use v5.26.0;
use strict;
use warnings FATAL => qw(all);

use Exporter              qw(import);
use File::Basename        qw(basename);
use File::Path            qw(mkpath);
use File::Spec::Functions qw(rel2abs);

use make::console;

our @EXPORT = qw(create_directory
                 execute
                 get_cpu_count
                 get_version
                 module_expand
                 module_shrink
                 read_config_file
                 write_config_file);

sub create_directory($$) {
	my ($location, $permissions) = @_;
	return eval {
		mkpath($location, 0, $permissions);
		return 1;
	} // 0;
}

sub execute(@) {
	say console_format "<|BOLD \$|> @_";
	return system @_;
}

sub get_version {
	state %version;
	return %version if %version;

	# Attempt to retrieve version information from src/version.sh
	chomp(my $vf = `sh src/version.sh 2>/dev/null`);
	if ($vf =~ /^InspIRCd-([0-9]+)\.([0-9]+)\.([0-9]+)(?:-(\w+))?$/) {
		%version = ( MAJOR => $1, MINOR => $2, PATCH => $3, LABEL => $4 );
	}

	# Attempt to retrieve missing version information from Git
	chomp(my $gr = `git describe --tags 2>/dev/null`);
	if ($gr =~ /^v([0-9]+)\.([0-9]+)\.([0-9]+)(?:[a-z]+\d+)?(?:-\d+-g(\w+))?$/) {
		$version{MAJOR} //= $1;
		$version{MINOR} //= $2;
		$version{PATCH} //= $3;
		$version{LABEL} = $4 if defined $4;
	}

	# If the user has specified a distribution label then we use it in
	# place of the label from src/version.sh or Git.
	$version{REAL_LABEL} = $version{LABEL};
	$version{LABEL} = shift // $version{LABEL};

	# If any of these fields are missing then the user has deleted the
	# version file and is not running from Git. Fill in the fields with
	# dummy data so we don't get into trouble with undef values later.
	$version{MAJOR} //= '0';
	$version{MINOR} //= '0';
	$version{PATCH} //= '0';

	# If there is no label then the user is using a stable release which
	# does not have a label attached.
	if (defined $version{LABEL}) {
		$version{FULL} = "$version{MAJOR}.$version{MINOR}.$version{PATCH}-$version{LABEL}"
	} else {
		$version{LABEL} = 'release';
		$version{FULL} = "$version{MAJOR}.$version{MINOR}.$version{PATCH}"
	}

	return %version;
}

sub get_cpu_count {
	my $count = 1;
	if ($^O =~ /bsd/) {
		$count = `sysctl -n hw.ncpu 2>/dev/null` || 1;
	} elsif ($^O eq 'darwin') {
		$count = `sysctl -n hw.activecpu 2>/dev/null` || 1;
	} elsif ($^O eq 'linux') {
		$count = `getconf _NPROCESSORS_ONLN 2>/dev/null` || 1;
	} elsif ($^O eq 'solaris') {
		$count = `psrinfo -p 2>/dev/null` || 1;
	}
	chomp($count);
	return $count;
}

sub module_expand($) {
	my $module = shift;
	$module = "$module.cpp" unless $module =~ /\.cpp$/;
	return $module;
}

sub module_shrink($) {
	my $module = shift;
	return basename($module,  '.cpp');
}

sub read_config_file($) {
	my $path = shift;
	my %config;
	open(my $fh, $path) or return %config;
	while (my $line = <$fh>) {
		next if $line =~ /^\s*($|\#)/;
		my ($key, $value) = ($line =~ /^(\S+)(?:\s(.*))?$/);
		$config{$key} = $value;
	}
	close $fh;
	return %config;
}

sub write_config_file($%) {
	my $path = shift;
	my %config = @_;
	open(my $fh, '>', $path) or print_error "unable to write to $path: $!";
	while (my ($key, $value) = each %config) {
		$value //= '';
		say $fh "$key $value";
	}
	close $fh;
}

1;
