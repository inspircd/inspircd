#!/usr/bin/perl
# Usage: pcre-config [--prefix] [--exec-prefix] [--version] [--libs] [--libs-posix] [--cflags] [--cflags-posix]
$data = `pcre-config --libs`;
$data =~ /-L(\S+)\s/;
$libpath = $1;
print "-Wl,--rpath -Wl,$libpath";

