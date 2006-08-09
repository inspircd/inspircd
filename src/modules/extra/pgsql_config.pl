#!/usr/bin/perl

my $v = substr(`pg_config --version`, 11);
my($a, $b, $c) = split(/\./, $v);

print "-D PGSQL_HAS_ESCAPECONN" if(hex(sprintf("%02x", $a) . sprintf("%02x", $b) . sprintf("%02x", $c)) >= 0x080104);
