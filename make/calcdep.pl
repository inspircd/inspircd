#!/usr/bin/perl
use strict;
use warnings;
use Getopt::Long;

my $basesrc = "$ENV{SOURCEPATH}/src";
my $baseinc = "$ENV{SOURCEPATH}/include";
my $baseout = `pwd`;
chomp $baseout;
chdir $basesrc;

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
			next if $inc eq 'inspircd_version.h' && $f eq $baseinc.'/inspircd.h';
			my $found = 0;
			for my $loc ("$basedir/$inc", "$baseinc/$inc") {
				next unless -e $loc;
				$found++;
				$dep{$loc}++;
				$dep{$_}++ for split / /, gendep $loc;
			}
			if ($found == 0 && $inc ne 'inspircd_win32wrapper.h') {
				print STDERR "WARNING: could not find header $inc for $f\n";
			} elsif ($found > 1 && $basedir ne $baseinc) {
				print STDERR "WARNING: ambiguous include $inc in $f\n";
			}
		}
	}
	close $in;
	$f2dep{$f} = join ' ', sort keys %dep;
	$f2dep{$f};
}

sub dep_cpp {
	my($file, $dfile) = @_;
	gendep $file;
	my($path,$base) = $file =~ m#^((?:.*/)?)([^/]+)\.cpp# or die "Bad file $file";
	my $ext = $path eq 'modules/' ? '.so' : '.o';
	my $out = "$path$base$ext";
	$dfile = "$baseout/$path.$base.d" unless defined $dfile;

	open OUT, '>', "$dfile" or die "Could not write $dfile: $!";
	print OUT "$out: $file $f2dep{$file}\n";
	print OUT "\t@\$(SOURCEPATH)/make/unit-cc.pl \$(VERBOSE) \$< $out\n";
}

sub dep_dir {
	my($dir, $dfile) = @_;
	if ($dir =~ m#^(.*?)([^/]+)/?$#) {
		my($path,$base) = ($1,$2);
		my $out = "$path$base.so";
		$dfile = "$baseout/$path.$base.d" unless defined $dfile;
		opendir DIR, "$basesrc/$dir";
		my $ofiles = join ' ', grep s/(.*)\.cpp$/$path$base\/$1.o/, readdir DIR;
		closedir DIR;
		open OUT, '>', "$dfile" or die "Could not write $dfile: $!";
		print OUT "$out: $ofiles\n\t\$(RUNCC) \$(PICLDFLAGS) -o \$\@ \$^\n";
		close OUT;
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
	print "Use: $0 {-all|-file src dest} [-quiet]\n";
	exit 1;
}

if (defined $file) {
	my $dfile = shift or die "Syntax: -file <in> <out>";
	$dfile = "$baseout/$dfile" unless $dfile =~ m#^/#;
	if (-f $file) {
		dep_cpp $file, $dfile;
	} elsif (-d $file) {
		dep_dir $file, $dfile;
	} else {
		print STDERR "Can't generate dependencies for $file\n";
		exit 1;
	}
} else {
	my @files = (<*.cpp>, <commands/*.cpp>, <modes/*.cpp>, <modules/*.cpp>, <modules/m_*/*.cpp>);
	push @files, "socketengines/$ENV{SOCKETENGINE}.cpp", "threadengines/threadengine_pthread.cpp";
	my @dirs = grep -d, <modules/m_*>;
	mkdir "$baseout/$_" for qw(commands modes modules socketengines threadengines), @dirs;

	for my $file (@files) {
		dep_cpp $file;
	}

	for my $dir (@dirs) {
		dep_dir $dir;
	}

	s#([^/]+)\.cpp#.$1.d# for @files;
	s#([^/]+)/?$#.$1.d# for @dirs;
	print join ' ', @files, @dirs;
}
