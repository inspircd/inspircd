%platform darwin
%platform freebsd
%platform netbsd
%platform openbsd
%target Makefile
#
# InspIRCd -- Internet Relay Chat Daemon
#
#   Copyright (C) 2017 Peter Powell <petpow@saberuk.com>
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

# This file will be installed as `Makefile` on BSD derivatives. When a user runs
# BSD Make it will be picked up as the default makefile even on systems like
# OpenBSD which have removed BSDMakefile support. If they run GNU Make then it
# will ignore this file and run GNUmakefile instead.

all clean configureclean debug deinstall distclean help install:
	@echo "InspIRCd no longer supports BSD Make. You should install GNU Make instead."
	@echo "If this is problematic for you then please contact us via our IRC channel"
	@echo "at irc.inspircd.org #InspIRCd."
	@exit 1
