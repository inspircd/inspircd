/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021 Benjamin Graillot <graillot@crans.org>
 *   Copyright (C) 2019 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2013, 2017-2018 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006, 2010 Craig Edwards <brain@inspircd.org>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/// $ModAuthor: Benjamin Graillot
/// $ModAuthorMail: graillot@crans.org
/// $ModDepends: core 3
/// $ModDesc: Hash provider for sha3.

/// $CompilerFlags: -Ivendor_directory("sha2")
/// $CompilerFlags: require_compiler("GCC") -Wno-long-long

#ifdef __GNUC__
# pragma GCC diagnostic push
#endif

// Fix warnings about the use of `long long` on C++03.
#if defined __clang__
# pragma clang diagnostic ignored "-Wc++11-long-long"
#elif defined __GNUC__
# pragma GCC diagnostic ignored "-Wlong-long"
#endif

#include "inspircd.h"
#include "modules/hash.h"

#include <sha3.h>

#ifdef __GNUC__
# pragma GCC diagnostic pop
#endif

class HashSHA3 : public HashProvider
{
 public:
	std::string GenerateRaw(const std::string& data) CXX11_OVERRIDE
	{
                SHA3 sha3;
                return std::string(sha3(data.data()), 64);
	}

	HashSHA3(Module* parent)
		: HashProvider(parent, "sha3", 64, 128)
	{
	}
};

class ModuleSHA3 : public Module
{
	HashSHA3 sha;
 public:
	ModuleSHA3() : sha(this)
	{
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Allows other modules to generate SHA-512 hashes.");
	}
};

MODULE_INIT(ModuleSHA3)
