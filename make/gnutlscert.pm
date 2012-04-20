#
# InspIRCd -- Internet Relay Chat Daemon
#
#   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
#   Copyright (C) 2007 Craig Edwards <craigedwards@brainbox.cc>
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


package make::gnutlscert;

require 5.8.0;

use strict;
use warnings FATAL => qw(all);

use Exporter 'import';
use make::configure;
our @EXPORT = qw(make_gnutls_cert);


sub make_gnutls_cert()
{
	open (FH, ">certtool.template");
	my $timestr = time();
	my $org = promptstring_s("Please enter the organization name", "My IRC Network");
	my $unit = promptstring_s("Please enter the unit Name", "Server Admins");
	my $state = promptstring_s("Pleae enter your state (two letter code)", "CA");
	my $country = promptstring_s("Please enter your country", "Oompa Loompa Land");
	my $commonname = promptstring_s("Please enter the certificate common name (hostname)", "irc.mynetwork.com");
	my $email = promptstring_s("Please enter a contact email address", "oompa\@loompa.com");
	print FH <<__END__;
# X.509 Certificate options
#
# DN options

# The organization of the subject.
organization = "$org"

# The organizational unit of the subject.
unit = "$unit"

# The locality of the subject.
# locality =

# The state of the certificate owner.
state = "$state"

# The country of the subject. Two letter code.
country = $country

# The common name of the certificate owner.
cn = "$commonname"

# A user id of the certificate owner.
#uid = "clauper"

# If the supported DN OIDs are not adequate you can set
# any OID here.
# For example set the X.520 Title and the X.520 Pseudonym
# by using OID and string pairs.
#dn_oid = "2.5.4.12" "Dr." "2.5.4.65" "jackal"

# This is deprecated and should not be used in new
# certificates.
# pkcs9_email = "none\@none.org"

# The serial number of the certificate
serial = $timestr

# In how many days, counting from today, this certificate will expire.
expiration_days = 700

# X.509 v3 extensions

# A dnsname in case of a WWW server.
#dns_name = "www.none.org"

# An IP address in case of a server.
#ip_address = "192.168.1.1"

# An email in case of a person
email = "$email"

# An URL that has CRLs (certificate revocation lists)
# available. Needed in CA certificates.
#crl_dist_points = "http://www.getcrl.crl/getcrl/"

# Whether this is a CA certificate or not
#ca

# Whether this certificate will be used for a TLS client
tls_www_client

# Whether this certificate will be used for a TLS server
tls_www_server

# Whether this certificate will be used to sign data (needed
# in TLS DHE ciphersuites).
signing_key

# Whether this certificate will be used to encrypt data (needed
# in TLS RSA ciphersuites). Note that it is prefered to use different
# keys for encryption and signing.
encryption_key

# Whether this key will be used to sign other certificates.
cert_signing_key

# Whether this key will be used to sign CRLs.
crl_signing_key

# Whether this key will be used to sign code.
code_signing_key

# Whether this key will be used to sign OCSP data.
ocsp_signing_key

# Whether this key will be used for time stamping.
time_stamping_key
__END__
close(FH);
if ( (my $status = system("certtool --generate-privkey --outfile key.pem")) ne 0) { return 1; }
if ( (my $status = system("certtool --generate-self-signed --load-privkey key.pem --outfile cert.pem --template certtool.template")) ne 0) { return 1; }
unlink("certtool.template");
return 0;
}

1;

