#!/usr/bin/perl

use lib "../..";
use make::utilities;

print make_rpath("mysql_config --libs_r");

