#!/usr/bin/perl
use strict;
use warnings;
BEGIN { push @INC, '..'; }
use make::configure;

my $file = shift;
my $verbose;

if ($file =~ /^-/) {
	$_ = $file;
	$file = shift;
	$verbose = /v/;
}

my $out = shift;

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
