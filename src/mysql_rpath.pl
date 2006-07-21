#!/usr/bin/perl
$data = `mysql_config --libs_r`;
$data =~ /-L(\S+)\s/;
$libpath = $1;
print "-Wl,--rpath -Wl,$libpath";

