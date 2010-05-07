#!/usr/bin/env perl
use strict;
use warnings;
BEGIN { push @INC, $ENV{SOURCEPATH}; }
use make::configure;

chdir $ENV{BUILDPATH};

my $type = shift;
my $out = shift;
my $verbose = ($type =~ s/-v$//);

if ($type eq 'gen-ld') {
	do_static_find(@ARGV);
} elsif ($type eq 'static-ld') {
	do_static_link(@ARGV);
} elsif ($type eq 'core-ld') {
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

sub do_static_find {
	my @flags;
	for my $file (@ARGV) {
		push @flags, getlinkerflags($file);
	}
	open F, '>', $out;
	print F join ' ', @flags;
	close F;
	exit 0;
}

sub do_static_link {
	my $execstr = "$ENV{RUNLD} -o $out $ENV{CORELDFLAGS} $ENV{LDLIBS}";
	for (@ARGV) {
		if (/\.cmd$/) {
			open F, '<', $_;
			my $libs = <F>;
			chomp $libs;
			$execstr .= ' '.$libs;
			close F;
		} else {
			$execstr .= ' '.$_;
		}
	}
	print "$execstr\n" if $verbose;
	exec $execstr;
}

sub do_core_link {
	my $execstr = "$ENV{RUNLD} -o $out $ENV{CORELDFLAGS} $ENV{LDLIBS} @_";
	print "$execstr\n" if $verbose;
	exec $execstr;
}

sub do_link_dir {
	my $execstr = "$ENV{RUNLD} -o $out $ENV{PICLDFLAGS} @_";
	print "$execstr\n" if $verbose;
	exec $execstr;
}

sub do_compile {
	my ($do_compile, $do_link, $file) = @_;

	my $flags = '';
	my $binary = $ENV{RUNCC};
	if ($do_compile) {
		$flags = $ENV{CXXFLAGS};
		$flags =~ s/ -pedantic// if nopedantic($file);
		$flags .= ' ' . getcompilerflags($file);

		if ($file =~ m#(?:^|/)((?:m|cmd)_[^/. ]+)(?:\.cpp|/.*\.cpp)$#) {
			$flags .= ' -DMODNAME='.$1.'.so';
		}
	} else {
		$binary = $ENV{RUNLD};
	}

	if ($do_link) {
		$flags = join ' ', $flags, $ENV{PICLDFLAGS}, getlinkerflags($file);
	} else {
		$flags .= ' -c';
	}

	my $execstr = "$binary -o $out $flags $file";
	print "$execstr\n" if $verbose;
	exec $execstr;
}
