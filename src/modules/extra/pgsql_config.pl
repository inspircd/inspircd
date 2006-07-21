#!/usr/bin/perl

my $v = substr(`pg_config --version`, 11);
my($a, $b, $c) = split(/\./, $v);

print "-D PGSQL_HAS_ESCAPECONN" if(($a >= 8) and ($b >= 1) and ($c >= 4));
