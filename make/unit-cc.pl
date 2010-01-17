#!/usr/bin/perl
use strict;
use warnings;
BEGIN { push @INC, $ENV{SOURCEPATH}; }
use make::configure;

chdir $ENV{BUILDPATH};

my $out = shift;
my $verbose;

if ($out =~ /^-/) {
	$_ = $out;
	$out = shift;
	$verbose = /v/;
	if (/f/) {
		do_static_find(@ARGV);
		exit;
	}
	if (/l/) {
		do_static_link(@ARGV);
		exit;
	}
}

my $file = shift;

my $cflags = $ENV{CXXFLAGS};
$cflags =~ s/ -pedantic// if nopedantic($file);
$cflags .= ' ' . getcompilerflags($file);

my $flags;
if ($out =~ /\.so$/) {
	$flags = join ' ', $cflags, $ENV{PICLDFLAGS}, getlinkerflags($file);
} else {
	$flags = "$cflags -c";
}

my $execstr = "$ENV{RUNCC} $flags -o $out $file";
print "$execstr\n" if $verbose;
exec $execstr;
exit 1;

sub do_static_find {
	my @flags;
	for my $file (@ARGV) {
		push @flags, getlinkerflags($file);
	}
	open F, '>', $out;
	print F join ' ', @flags;
	close F;
}

sub do_static_link {
	my $execstr = "$ENV{RUNCC} -o $out $ENV{CORELDFLAGS} $ENV{LDLIBS}";
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
	exit 1;
}
