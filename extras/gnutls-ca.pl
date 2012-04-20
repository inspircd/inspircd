#!/usr/bin/perl

#
# InspIRCd -- Internet Relay Chat Daemon
#
#   Copyright (C) 2010 Daniel De Graaf <danieldg@inspircd.org>
#
# This file is part of InspIRCd.  InspIRCd is free software: you can
# redistribute it and/or modify it under the terms of the GNU General Public
# License as published by the Free Software Foundation, version 2.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
# details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#


use strict;
use warnings;

sub gen_key;
sub gen_ca;
sub gen_server;
sub ca_sign;
sub gen_templates;
sub key_info;;

mkdir "ssl";
chdir "ssl" or die $!;
mkdir "keys" and chmod 0700, "keys";
mkdir "certs";
gen_templates;

my $what = shift || '';
if ($what eq 'sign') {
	gen_ca;
	for my $req (@ARGV) {
		my $out = $req;
		$out =~ s/request/ca-sign/ or ($out .= '.sign');
		ca_sign $req, $out;
	}
} elsif ($what eq 'server') {
	gen_ca;
	gen_server;
	ca_sign 'server-request.pem', 'server-ca-sign.pem';
	print <<END;
SSL certificates have been generated.

It is a good idea to make your SSL certificate match your IRCd's hostname; some
IRC clients complain about a mismatch here. To do this, edit ssl/server.info
and rerun this script.

If you just want a simple server certificate, copy ssl/server-key.pem and
ssl/server-selfsign.pem to conf/key.pem and conf/cert.pem.

If you want a certificate signed by a CA, use ssl/server-key.pem and
ssl/server-ca-sign.pem. If you have multiple servers, choose one to hold the
root CA and copy the other server-request.pem files to that system. Then use
$0 sign <filename> to sign them with the single CA.
The file ssl/ca.pem can be used by clients to verify your servers.
END
} else {
	print "Use: \n";
	print " $0 server        Generate server certificates for one server\n";
	print " $0 sign cert     Sign another server's request with the local CA\n";
	exit (@ARGV != 0);
}

sub gen_key {
	my $key = shift;
	return if -e $key;
	print "[\e[32m*\e[0m] Generating private key $key\n";

	system "certtool --generate-privkey --outfile keys/tmp.pem 2>>log" and die;
	my $fp;
	open my $info, '-|', 'certtool --key-info < keys/tmp.pem' or die;
	while (<$info>) {
		m#Public Key ID: ([0-9A-F:]+)# and $fp = $1;
	}
	$fp or die "Cannot read key ID of the key we just made";
	$fp =~ s/://g;
	$fp = lc $fp;
	rename 'keys/tmp.pem', "keys/$fp.pem";
	unlink $key;
	symlink "keys/$fp.pem", $key;
}

sub gen_ca {
	gen_key 'ca-key.pem';
	my @ca = stat 'ca.pem';
	my @ca_t = stat 'ca.info';
	if (!@ca || $ca[9] < $ca_t[9]) {
		print "[\e[32m*\e[0m] Creating certificate authority (ca.pem)\n";
		system "certtool --generate-self-signed --template ca.info --load-privkey ca-key.pem --outfile tmp.pem 2>>log" and die;
		my($fn, $certfp, $keyfp) = key_info;

		unlink "ca.pem";
		symlink $fn, "ca.pem";
		@ca = stat 'ca.pem';
	}
}

sub gen_server {
	gen_key 'server-key.pem';

	my @server_t = stat 'server.info';
	my @server_req = stat 'server-request.pem';
	my @server_ss = stat 'server-selfsign.pem';

	if (!@server_req || $server_req[9] < $server_t[9]) {
		print "[\e[32m*\e[0m] Creating server certificate request (server-request.pem)\n";
		system "certtool --generate-request --template server.info --load-privkey server-key.pem --outfile server-request.pem 2>>log" and die;
		@server_req = stat 'server-request.pem';
	}

	if (!@server_ss || $server_ss[9] < $server_t[9]) {
		print "[\e[32m*\e[0m] Creating self-signed server certificate (server-selfsign.pem)\n";
		system "certtool --generate-self-signed --template server.info --load-privkey server-key.pem --outfile tmp.pem 2>>log" and die;
		my($fn, $certfp, $keyfp) = key_info;

		unlink "server-selfsign.pem";
		symlink $fn, "server-selfsign.pem";
	}
}

sub ca_sign {
	my($in, $out) = @_;
	my @ca = stat 'ca.pem';
	my @server_req = stat $in;
	my @server_cs = stat $out;
	if (!@server_cs || $server_cs[9] < $server_req[9] || $server_cs[9] < $ca[9]) {
		print "[\e[32m*\e[0m] Signing $in with ca.pem ($out)\n";
		system "certtool --generate-certificate --load-request '$in' --load-ca-certificate ca.pem --load-ca-privkey ca-key.pem --template ca-signing.info --outfile tmp.pem 2>>log" and die;
		my($fn, $certfp, $keyfp) = key_info;

		system "cat ca.pem $fn > certs/$certfp-full.pem";
		unlink $out;
		symlink "certs/$certfp-full.pem", $out;
	}
}

sub gen_templates {
	if (!-e 'server.info') {
		open F, '>', 'server.info';
		print F <<EOF;
# X.509 Certificate options

# CN (common name) - the name of your server. You want to set this.
#cn = "irc.example.com"

# How many days, counting from today, will certificate be valid?
# Default is 1 year
#expiration_days = 1000

#You can also set other fields, see certtool documentation for the rest
EOF
		close F;
	}
	if (!-e 'ca.info') {
		open F, '>', 'ca.info';
		print F <<EOF;
# X.509 Certificate options
# This is for a Certificate Authority
ca

# CN (common name) - the name of your root.
# While not strictly required, your IRC network is good to put here
#cn = "irc.example.com"

# How many days, counting from today, will certificate be valid?
# Default is 1 year
#expiration_days = 1000

#You can also set other fields, see certtool documentation for the rest
EOF
		close F;
	}
	if (!-e 'ca-signing.info') {
		open F, '>', 'ca-signing.info';
		print F <<EOF;
# X.509 Certificate options
# This is for certificate requests being signed by the CA.
# Blank to accept all fields from the request.
EOF
		close F;
	}
}

sub key_info {
	my($certfp, $keyfp);
	open my $info, '-|', 'certtool --certificate-info < tmp.pem' or die;
	while (<$info>) {
		if (m#SHA-1 fingerprint:#) {
			$certfp = <$info>;
		} elsif (m#Public Key Id:#) {
			$keyfp = <$info>;
		}
	}
	chomp($certfp, $keyfp);
	$certfp =~ s/^\t+//;
	$keyfp =~ s/^\t+//;
	my $fn = "certs/$certfp.pem";
	rename 'tmp.pem', $fn;
	return($fn, $certfp, $keyfp);
}
