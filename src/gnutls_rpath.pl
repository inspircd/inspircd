#!/usr/bin/perl
$data = `libgnutls-config --libs`;
$data =~ /-L(\S+)\s/;
$libpath = $1;
print "-Wl,--rpath -Wl,$libpath";

