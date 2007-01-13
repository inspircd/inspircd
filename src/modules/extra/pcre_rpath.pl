#!/usr/bin/perl

use lib "../..";
use make::utilities;

print make_rpath("pcre-config --libs");

