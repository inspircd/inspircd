#!/usr/bin/perl

`pg_config --version` =~ /^.*?(\d+)\.(\d+)\.(\d+).*?$/;
print "-D PGSQL_HAS_ESCAPECONN" if(hex(sprintf("0x%02x%02x%02x", $1, $2, $3)) >= 0x080104);
