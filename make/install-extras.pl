#!/usr/bin/perl
use strict;
use warnings;

my $mode = shift;
my %installed;

for my $dir (qw(src src/modules)) {
	opendir(DIRHANDLE, $dir);
	for my $file (sort readdir(DIRHANDLE)) {
		next unless $file =~ /\.cpp$/;
		open CPP, '<', "$dir/$file" or die "Can't open $dir/$file to scan it: $!";
		while (<CPP>) {
			if (/\/\* \$CopyInstall: (\S+) (\S+) \*\//i) {
				my($ifile, $idir) = ($1,$2);
				next if exists $installed{$1.' '.$2};
				$installed{$1.' '.$2}++;
				$idir =~ s/\$\(([^)]+)\)/$ENV{$1}/eg;
				if ($mode eq 'install') {
					system "install $ifile $idir";
				} else {
					$ifile =~ s/.*\///g;
					system "rm $idir/$ifile";
				}
			}
		}
	}
	closedir(DIRHANDLE);
}
