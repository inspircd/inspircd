#!/usr/bin/perl

#       +------------------------------------+
#       | Inspire Internet Relay Chat Daemon |
#       +------------------------------------+
#
#  InspIRCd: (C) 2002-2007 InspIRCd Development Team
# See: http://www.inspircd.org/wiki/index.php/Credits
#
#  This program is free but copyrighted software; see
#          the file COPYING for details.
#
# ---------------------------------------------------

use strict;
use warnings;

my $maxparams = shift;

die "You must supply a number of parameters to generate headers allowing for!" unless(defined $maxparams);
die "You must request a non-negative parameter limit!" unless($maxparams >= 0);

print STDERR "Generating headerfile for a maximium of $maxparams parameters\n";

# First generate the HanderBase family

my @templatetypes = ('ReturnType');
for(my $i = 0; $i <= $maxparams; $i++)
{
	push @templatetypes, "Param" . $i if($i > 0);
	print "template <typename " . join(', typename ', @templatetypes) . "> class CoreExport HandlerBase" . $i . "\n";
	print "{\n";
	print " public:\n";
	print "	virtual ReturnType Call(" . join(', ', @templatetypes[1..$#templatetypes]) . ") = 0;\n";
	print "	virtual ~HandlerBase" . $i . "() { }\n";
	print "};\n\n";
}

# And now the caller family

print "template <typename HandlerType> class CoreExport caller\n";
print "{\n";
print " public:\n";
print "	HandlerType* target;\n\n";
print "	caller(HandlerType* initial)\n";
print "	: target(initial)\n";
print "	{ }\n\n";
print "	virtual ~caller() { }\n\n";
print "	caller& operator=(HandlerType* newtarget)\n";
print "	{\n";
print "		target = newtarget;\n";
print "		return *this;\n";
print "	}\n";
print "};\n\n";




@templatetypes = ('ReturnType');
for(my $i = 0; $i <= $maxparams; $i++)
{
	push @templatetypes, "Param" . $i if($i > 0);
	
	my $handlertype = "HandlerBase" . $i . "<" . join(', ', @templatetypes) . ">";
	my @templatetypepairs = map { $_ . " " . lc($_) }  @templatetypes;
	my @lctemplatetypes = map(lc, @templatetypes);
	
	print "template <typename " . join(', typename ', @templatetypes) . "> class CoreExport caller" . $i . " : public caller< " . $handlertype . " >\n";
	print "{\n";
	print " public:\n";
	print "	caller" . $i . "(" . $handlertype . "* initial)\n";
	print "	: caller< " . $handlertype. " >::caller(initial)\n";
	print "	{ }\n\n";
	print "	virtual ReturnType operator() (" . join(', ', @templatetypepairs[1..$#templatetypepairs]) . ")\n";
	print "	{\n";
	print "		return this->target->Call(" . join(', ', @lctemplatetypes[1..$#lctemplatetypes]) . ");\n";
	print "	}\n";
	print "};\n\n";
}

