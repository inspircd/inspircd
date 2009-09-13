#!/usr/bin/perl
use strict;
use warnings;
use Getopt::Long;

my %f2dep;

sub gendep;
sub gendep {
	my $f = shift;
	my $basedir = $f =~ m#(.*)/# ? $1 : '.';
	return $f2dep{$f} if exists $f2dep{$f};
	$f2dep{$f} = '';
	my %dep;
	my $link = readlink $f;
	if (defined $link) {
		$link = "$basedir/$link" unless $link =~ m#^/#;
		$dep{$link}++;
	}
	open my $in, '<', $f or die "Could not read $f";
	while (<$in>) {
		if (/^\s*#\s*include\s*"([^"]+)"/) {
			my $inc = $1;
			next if $inc eq 'inspircd_version.h' && $f eq '../include/inspircd.h';
			my $found = 0;
			for my $loc ("$basedir/$inc", "../include/$inc") {
				next unless -e $loc;
				$found++;
				$dep{$loc}++;
				$dep{$_}++ for split / /, gendep $loc;
			}
			if ($found == 0 && $inc ne 'inspircd_win32wrapper.h') {
				print STDERR "WARNING: could not find header $inc for $f\n";
			} elsif ($found > 1 && $basedir ne '../include') {
				print STDERR "WARNING: ambiguous include $inc in $f\n";
			}
		}
	}
	close $in;
	$f2dep{$f} = join ' ', sort keys %dep;
	$f2dep{$f};
}

sub dep_cpp {
	my $file = shift;
	gendep $file;
	my($path,$base) = $file =~ m#^((?:.*/)?)([^/]+)\.cpp# or die "Bad file $file";
	my $cmd = "$path.$base.d";
	my $ext = $path eq 'modules/' || $path eq 'commands/' ? '.so' : '.o';
	my $out = "$path$base$ext";

	open OUT, '>', $cmd;
	print OUT "$out: $file $f2dep{$file}\n";
	print OUT "\t@../make/unit-cc.pl \$(VERBOSE) $file $out\n";
	print OUT "$cmd: $file $f2dep{$file}\n";
	print OUT "\t../make/calcdep.pl -file $file\n";
}

sub dep_dir {
	my $dir = shift;
	if ($dir =~ m#^(.*?)([^/]+)/?$#) {
		my($path,$base) = ($1,$2);
		my $cmd = "$path.$base.d";
		my $out = "$path$base.so";
		opendir DIR, $dir;
		my $ofiles = join ' ', grep s/(.*)\.cpp$/$path$base\/$1.o/, readdir DIR;
		closedir DIR;
		open OUT, '>', $cmd;
		print OUT "$out: $ofiles\n\t".'$(RUNCC) $(PICLDFLAGS) -o $@ '
			.$ofiles."\n";
		print OUT "$cmd: $dir\n\t".'@../make/calcdep.pl -file '."$path$base\n";
	} else {
		print STDERR "Cannot generate depencency for $dir\n";
		exit 1;
	}
}

my($all,$quiet, $file);
GetOptions(
	'all' => \$all,
	'quiet' => \$quiet,
	'file=s' => \$file,
);

if (!$all && !defined $file) {
	print "Use: $0 {-all|-file filename} [-quiet]\n";
	exit 1;
}

if (defined $file) {
	if (-f $file) {
		dep_cpp $file;
	} elsif (-d $file) {
		dep_dir $file;
	} else {
		print STDERR "Can't generate dependencies for $file\n";
		exit 1;
	}
} else {
	my @files = (<*.cpp>, <commands/*.cpp>, <modes/*.cpp>, <modules/*.cpp>, <modules/m_*/*.cpp>);
	push @files, "socketengines/$ENV{SOCKETENGINE}.cpp", "threadengines/threadengine_pthread.cpp";
	for my $file (@files) {
		dep_cpp $file;
	}

	my @dirs = grep -d, <modules/m_*>;
	for my $dir (@dirs) {
		dep_dir $dir;
	}

	s#([^/]+)\.cpp#.$1.d# for @files;
	s#([^/]+)/?$#.$1.d# for @dirs;
	print join ' ', @files, @dirs;
}
