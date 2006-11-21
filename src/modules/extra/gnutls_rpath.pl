#!/usr/bin/perl
$data = `libgnutls-config --libs`;
$data =~ s/-L(\S+)\s/-Wl,--rpath -Wl,$1 -L$1 /g;
print "$data";
