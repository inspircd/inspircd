#!/usr/bin/perl
use strict;
use warnings;

# This used to be a wrapper around cc -M; however, this is a very slow
# operation and we don't conditionally include our own files often enough
# to justify the full preprocesor invocation for all ~200 files.

my %f2dep;

sub gendep;
sub gendep {
	my $f = shift;
	my $basedir = $f =~ m#(.*)/# ? $1 : '.';
	return $f2dep{$f} if exists $f2dep{$f};
	$f2dep{$f} = '';
	my %dep;
	open my $in, '<', $f;
	while (<$in>) {
		if (/^\s*#\s*include\s*"([^"]+)"/) {
			my $inc = $1;
			for my $loc ("$basedir/$inc", "../include/$inc") {
				next unless -e $loc;
				$dep{$loc}++;
				$dep{$_}++ for split / /, gendep $loc;
			}
		}
	}
	close $in;
	$f2dep{$f} = join ' ', sort keys %dep;
	$f2dep{$f};
}

for my $file (@ARGV) {
	if (-e $file && $file =~ /cpp$/) {
		gendep $file;
		my($path,$base) = $file =~ m#^((?:.*/)?)([^/]+)\.cpp#;
		my $cmd = "$path.$base.d";
		my $ext = $path eq 'modules/' || $path eq 'commands/' ? '.so' : '.o';
		my $out = "$path$base$ext";

		open OUT, '>', $cmd;
		print OUT "$out: $file $f2dep{$file}\n";
		print OUT "\t@../make/unit-cc.pl \$(VERBOSE) $file $out\n";
		print OUT "$cmd: $file $f2dep{$file}\n";
		print OUT "\t../make/calcdep.pl $file\n";
	} elsif (-d $file && $file =~ m#^(.*?)([^/]+)/?$#) {
		my($path,$base) = ($1,$2);
		my $cmd = "$path.$base.d";
		my $out = "$path$base.so";
		opendir DIR, $file;
		my $ofiles = join ' ', grep s#(.*)\.cpp$#$path$base/$1.o#, readdir DIR;
		closedir DIR;
		open OUT, '>', $cmd;
		print OUT "$out: $ofiles\n\t".'$(RUNCC) $(PICLDFLAGS) -o $@ '
			.$ofiles."\n";
		print OUT "$cmd: $file\n\t".'@../make/calcdep.pl '."$path$base\n";
	} else {
		print "Cannot generate depencency for $file\n";
		exit 1;
	}
}
