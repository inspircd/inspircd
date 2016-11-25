#
# InspIRCd -- Internet Relay Chat Daemon
#
#   Copyright (C) 2012-2014 Peter Powell <petpow@saberuk.com>
#   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
#   Copyright (C) 2007-2008 Craig Edwards <craigedwards@brainbox.cc>
#   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
#   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
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

package make::configure;

use feature ':5.10';
use strict;
use warnings FATAL => qw(all);

use Cwd                   qw(getcwd);
use Exporter              qw(import);
use File::Basename        qw(basename dirname);
use File::Spec::Functions qw(catfile);

use make::common;
use make::console;

use constant CONFIGURE_DIRECTORY     => '.configure';
use constant CONFIGURE_CACHE_FILE    => catfile(CONFIGURE_DIRECTORY, 'cache.cfg');
use constant CONFIGURE_CACHE_VERSION => '1';

our @EXPORT = qw(CONFIGURE_CACHE_FILE
                 CONFIGURE_CACHE_VERSION
                 cmd_clean
                 cmd_help
                 cmd_update
                 run_test
                 test_file
                 test_header
                 read_configure_cache
                 write_configure_cache
                 get_compiler_info
                 find_compiler
                 parse_templates);

sub __get_socketengines {
	my @socketengines;
	foreach (<src/socketengines/socketengine_*.cpp>) {
		s/src\/socketengines\/socketengine_(\w+)\.cpp/$1/;
		push @socketengines, $1;
	}
	return @socketengines;
}

# TODO: when buildtool is done this can be mostly removed with
#       the remainder being merged into parse_templates.
sub __get_template_settings($$$) {

	# These are actually hash references
	my ($config, $compiler, $version) = @_;

	# Start off by populating with the config
	my %settings = %$config;

	# Compiler information
	while (my ($key, $value) = each %{$compiler}) {
		$settings{'COMPILER_' . $key} = $value;
	}

	# Version information
	while (my ($key, $value) = each %{$version}) {
		$settings{'VERSION_' . $key} = $value;
	}

	# Miscellaneous information
	$settings{CONFIGURE_DIRECTORY} = CONFIGURE_DIRECTORY;
	$settings{CONFIGURE_CACHE_FILE} = CONFIGURE_CACHE_FILE;
	$settings{SYSTEM_NAME} = lc $^O;
	chomp($settings{SYSTEM_NAME_VERSION} = `uname -sr 2>/dev/null`);

	return %settings;
}

sub __test_compiler($) {
	my $compiler = shift;
	return 0 unless run_test("`$compiler`", !system "$compiler -v >/dev/null 2>&1");
	return 0 unless run_test("`$compiler`", test_file($compiler, 'compiler.cpp', '-fno-rtti'), 'compatible');
	return 1;
}

sub cmd_clean {
	unlink CONFIGURE_CACHE_FILE;
}

sub cmd_help {
	my $PWD = getcwd();
	my $SELIST = join ', ', __get_socketengines();
	print <<EOH;
Usage: $0 [options]

When no options are specified, configure runs in interactive mode and you must
specify any required values manually. If one or more options are specified,
non-interactive configuration is started and any omitted values are defaulted.

PATH OPTIONS

  --system                      Automatically set up the installation paths
                                for system-wide installation.
  --prefix=[dir]                The root install directory. If this is set then
                                all subdirectories will be adjusted accordingly.
                                [$PWD/run]
  --binary-dir=[dir]            The location where the main server binary is
                                stored.
                                [$PWD/run/bin]
  --config-dir=[dir]            The location where the configuration files and
                                SSL certificates are stored.
                                [$PWD/run/conf]
  --data-dir=[dir]              The location where the data files, such as the
                                pid file, are stored.
                                [$PWD/run/data]
  --log-dir=[dir]               The location where the log files are stored.
                                [$PWD/run/logs]
  --manual-dir=[dir]            The location where the manual files are stored.
                                [$PWD/run/manuals]
  --module-dir=[dir]            The location where the loadable modules are
                                stored.
                                [$PWD/run/modules]

EXTRA MODULE OPTIONS

  --enable-extras=[extras]      Enables a comma separated list of extra modules.
  --disable-extras=[extras]     Disables a comma separated list of extra modules.
  --list-extras                 Shows the availability status of all extra
                                modules.

MISC OPTIONS

  --clean                       Remove the configuration cache file and start
                                the interactive configuration wizard.
  --disable-interactive         Disables the interactive configuration wizard.
  --distribution-label=[text]   Sets a distribution specific version label in
                                the build configuration.
  --gid=[id|name]               Sets the group to run InspIRCd as.
  --help                        Show this message and exit.
  --socketengine=[name]         Sets the socket engine to be used. Possible
                                values are $SELIST.
  --uid=[id|name]               Sets the user to run InspIRCd as.
  --update                      Updates the build environment with the settings
                                from the cache.


FLAGS

  CXX=[name]                    Sets the C++ compiler to use when building the
                                server. If not specified then the build system
                                will search for c++, g++, clang++ or icpc.

If you have any problems with configuring InspIRCd then visit our IRC channel
at irc.inspircd.org #InspIRCd for support.

EOH
	exit 0;
}

sub cmd_update {
	print_error "You have not run $0 before. Please do this before trying to update the generated files." unless -f CONFIGURE_CACHE_FILE;
	say 'Updating...';
	my %config = read_configure_cache();
	my %compiler = get_compiler_info($config{CXX});
	my %version = get_version $config{DISTRIBUTION};
	parse_templates(\%config, \%compiler, \%version);
	say 'Update complete!';
	exit 0;
}

sub run_test($$;$) {
	my ($what, $result, $adjective) = @_;
	$adjective //= 'available';
	print_format "Checking whether <|GREEN $what|> is $adjective ... ";
	print_format $result ? "<|GREEN yes|>\n" : "<|RED no|>\n";
	return $result;
}

sub test_file($$;$) {
	my ($compiler, $file, $args) = @_;
	my $status = 0;
	$args //= '';
	$status ||= system "$compiler -o __test_$file make/test/$file $args >/dev/null 2>&1";
	$status ||= system "./__test_$file >/dev/null 2>&1";
	unlink "./__test_$file";
	return !$status;
}

sub test_header($$;$) {
	my ($compiler, $header, $args) = @_;
	$args //= '';
	open(COMPILER, "| $compiler -E - $args >/dev/null 2>&1") or return 0;
	print COMPILER "#include <$header>";
	close(COMPILER);
	return !$?;
}

sub read_configure_cache {
	my %config;
	open(CACHE, CONFIGURE_CACHE_FILE) or return %config;
	while (my $line = <CACHE>) {
		next if $line =~ /^\s*($|\#)/;
		my ($key, $value) = ($line =~ /^(\S+)(?:\s(.*))?$/);
		$config{$key} = $value;
	}
	close(CACHE);
	return %config;
}

sub write_configure_cache(%) {
	unless (-e CONFIGURE_DIRECTORY) {
		print_format "Creating <|GREEN ${\CONFIGURE_DIRECTORY}|> ...\n";
		create_directory CONFIGURE_DIRECTORY, 0750 or print_error "unable to create ${\CONFIGURE_DIRECTORY}: $!";
	}

	print_format "Writing <|GREEN ${\CONFIGURE_CACHE_FILE}|> ...\n";
	my %config = @_;
	open(CACHE, '>', CONFIGURE_CACHE_FILE) or print_error "unable to write ${\CONFIGURE_CACHE_FILE}: $!";
	while (my ($key, $value) = each %config) {
		$value //= '';
		say CACHE "$key $value";
	}
	close(CACHE);
}

sub get_compiler_info($) {
	my $binary = shift;
	my $version = `$binary -v 2>&1`;
	if ($version =~ /Apple\sLLVM\sversion\s(\d+\.\d+)/i) {
		# Apple version their LLVM releases slightly differently to the mainline LLVM.
		# See https://trac.macports.org/wiki/XcodeVersionInfo for more information.
		return (NAME => 'AppleClang', VERSION => $1);
	} elsif ($version =~ /clang\sversion\s(\d+\.\d+)/i) {
		return (NAME => 'Clang', VERSION => $1);
	} elsif ($version =~ /gcc\sversion\s(\d+\.\d+)/i) {
		return (NAME => 'GCC', VERSION => $1);
	} elsif ($version =~ /(?:icc|icpc)\sversion\s(\d+\.\d+).\d+\s\(gcc\sversion\s(\d+\.\d+).\d+/i) {
		return (NAME => 'ICC', VERSION => $1);
	}
	return (NAME => $binary, VERSION => '0.0');
}

sub find_compiler {
	my @compilers = qw(c++ g++ clang++ icpc);
	foreach my $compiler (shift // @compilers) {
		return $compiler if __test_compiler $compiler;
		return "xcrun $compiler" if $^O eq 'darwin' && __test_compiler "xcrun $compiler";
	}
}

sub parse_templates($$$) {

	# These are actually hash references
	my ($config, $compiler, $version) = @_;

	# Collect settings to be used when generating files
	my %settings = __get_template_settings($config, $compiler, $version);

	# Iterate through files in make/template.
	foreach (<make/template/*>) {
		print_format "Parsing <|GREEN $_|> ...\n";
		open(TEMPLATE, $_) or print_error "unable to read $_: $!";
		my (@lines, $mode, @platforms, %targets);

		# First pass: parse template variables and directives.
		while (my $line = <TEMPLATE>) {
			chomp $line;

			# Does this line match a variable?
			while ($line =~ /(@(\w+?)@)/) {
				my ($variable, $name) = ($1, $2);
				if (defined $settings{$name}) {
					$line =~ s/\Q$variable\E/$settings{$name}/;
				} else {
					print_warning "unknown template variable '$name' in $_!";
					last;
				}
			}

			# Does this line match a directive?
			if ($line =~ /^\s*%(\w+)\s+(.+)$/) {
				if ($1 eq 'define') {
					if ($settings{$2}) {
						push @lines, "#define $2";
					} else {
						push @lines, "#undef $2";
					}
				} elsif ($1 eq 'mode') {
					$mode = oct $2;
				} elsif ($1 eq 'platform') {
					push @platforms, $2;
				} elsif ($1 eq 'target') {
					if ($2 =~ /(\w+)\s(.+)/) {
						$targets{$1} = $2;
					} else {
						$targets{DEFAULT} = $2;
					}
				} else {
					print_warning "unknown template command '$1' in $_!";
					push @lines, $line;
				}
				next;
			}
			push @lines, $line;
		}
		close(TEMPLATE);

		# Only proceed if this file should be templated on this platform.
		if ($#platforms < 0 || grep { $_ eq $^O } @platforms) {

			# Add a default target if the template has not defined one.
			unless (scalar keys %targets) {
				$targets{DEFAULT} = catfile(CONFIGURE_DIRECTORY, basename $_);
			}

			# Second pass: parse makefile junk and write files.
			while (my ($name, $target) = each %targets) {

				# TODO: when buildtool is done this mess can be removed completely.
				my @final_lines;
				foreach my $line (@lines) {

					# Are we parsing a makefile and does this line match a statement?
					if ($name =~ /(?:BSD|GNU)_MAKE/ && $line =~ /^\s*\@(\w+)(?:\s+(.+))?$/) {
						my @tokens = split /\s/, $2 if defined $2;
						if ($1 eq 'DO_EXPORT' && defined $2) {
							if ($name eq 'BSD_MAKE') {
								foreach my $variable (@tokens) {
									push @final_lines, "MAKEENV += $variable='\${$variable}'";
								}
							} elsif ($name eq 'GNU_MAKE') {
								push @final_lines, "export $2";
							}
						} elsif ($1 eq 'ELSE') {
							if ($name eq 'BSD_MAKE') {
								push @final_lines, ".else";
							} elsif ($name eq 'GNU_MAKE') {
								push @final_lines, "else";
							}
						} elsif ($1 eq 'ENDIF') {
							if ($name eq 'BSD_MAKE') {
								push @final_lines, ".endif";
							} elsif ($name eq 'GNU_MAKE') {
								push @final_lines, "endif";
							}
						} elsif ($1 eq 'ELSIFEQ' && defined $2) {
							if ($name eq 'BSD_MAKE') {
								push @final_lines, ".elif $tokens[0] == $tokens[1]";
							} elsif ($name eq 'GNU_MAKE') {
								push @final_lines, "else ifeq ($tokens[0], $tokens[1])";
							}
						} elsif ($1 eq 'IFDEF' && defined $2) {
							if ($name eq 'BSD_MAKE') {
								push @final_lines, ".if defined($2)";
							} elsif ($name eq 'GNU_MAKE') {
								push @final_lines, "ifdef $2";
							}
						} elsif ($1 eq 'IFEQ' && defined $2) {
							if ($name eq 'BSD_MAKE') {
								push @final_lines, ".if $tokens[0] == $tokens[1]";
							} elsif ($name eq 'GNU_MAKE') {
								push @final_lines, "ifeq ($tokens[0],$tokens[1])";
							}
						} elsif ($1 eq 'IFNEQ' && defined $2) {
							if ($name eq 'BSD_MAKE') {
								push @final_lines, ".if $tokens[0] != $tokens[1]";
							} elsif ($name eq 'GNU_MAKE') {
								push @final_lines, "ifneq ($tokens[0],$tokens[1])";
							}
						} elsif ($1 eq 'IFNDEF' && defined $2) {
							if ($name eq 'BSD_MAKE') {
								push @final_lines, ".if !defined($2)";
							} elsif ($name eq 'GNU_MAKE') {
								push @final_lines, "ifndef $2";
							}
						} elsif ($1 eq 'TARGET' && defined $2) {
							if ($tokens[0] eq $name) {
								push @final_lines, substr($2, length($tokens[0]) + 1);
							}
						} elsif ($1 !~ /[A-Z]/) {
							# HACK: silently ignore if lower case as these are probably make commands.
							push @final_lines, $line;
						} else {
							print_warning "unknown template command '$1' in $_!";
							push @final_lines, $line;
						}
						next;
					}

					push @final_lines, $line;
				}

				# Create the directory if it doesn't already exist.
				my $directory = dirname $target;
				unless (-e $directory) {
					print_format "Creating <|GREEN $directory|> ...\n";
					create_directory $directory, 0750 or print_error "unable to create $directory: $!";
				};

				# Write the template file.
				print_format "Writing <|GREEN $target|> ...\n";
				open(TARGET, '>', $target) or print_error "unable to write $target: $!";
				foreach (@final_lines) {
					say TARGET $_;
				}
				close(TARGET);

				# Set file permissions.
				if (defined $mode) {
					chmod $mode, $target;
				}
			}
		}
	}
}

1;
