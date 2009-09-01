#!/usr/bin/perl
use strict;
BEGIN { push @INC, '..'; }
use make::configure;

my $file = shift;

$file =~ /(.*)\.cpp$/ or die "Cannot process $file";
my $base = $1;

my $out = "$base.d";

open IN, '<', $file or die "Could not read $file: $!";
open OUT, '>', $out or die "Could not write $out: $!";

my $cc_deps = qx($ENV{CC} $ENV{FLAGS} -MM $file);
$cc_deps =~ s/.*?:\s*//;

my $ext = $file =~ m#(modules|commands)/[^/]+$# ? '.so' : '.o';
print OUT "$base$ext: $cc_deps";
print OUT "\t@../make/unit-cc.pl \$(VERBOSE) $file $base$ext\n";
print OUT "$base.d: $cc_deps";
print OUT "\t\@\$(VDEP_IN)\n";
print OUT "\t../make/calcdep.pl $file\n";
print OUT "\t\@\$(VDEP_OUT)\n";
