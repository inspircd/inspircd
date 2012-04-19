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

package make::opensslcert;

require 5.8.0;

use strict;
use warnings FATAL => qw(all);

use Exporter 'import';
use make::configure;
our @EXPORT = qw(make_openssl_cert);


sub make_openssl_cert()
{
	open (FH, ">openssl.template");
	my $org = promptstring_s("Please enter the organization name", "My IRC Network");
	my $unit = promptstring_s("Please enter the unit Name", "Server Admins");
	my $country = promptstring_s("Please enter your country (two letter code)", "US");
	my $state = promptstring_s("Please enter your state or locality name", "Alaska");
	my $city = promptstring_s("Please enter your city", "Factory Town");
	my $email = promptstring_s("Please enter a contact email address", "oompa\@loompa.com");
	my $commonname = promptstring_s("Please enter the common name (domain name) of the irc server", "example.inspircd.org");
	print FH <<__END__;
$country
$state
$city
$org
$unit
$commonname
$email
__END__
close(FH);

my $time = promptstring_s("Please enter the number of days that this certificate is valid for","365");

system("cat openssl.template | openssl req -x509 -nodes -newkey rsa:1024 -keyout key.pem -out cert.pem -days $time 2>/dev/null");
system("openssl dhparam -out dhparams.pem 1024");
unlink("openssl.template");
}

1;
