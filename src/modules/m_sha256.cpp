/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Craig Edwards <craigedwards@brainbox.cc>
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

#include <sha2.c>

#ifdef __GNUC__
# pragma GCC diagnostic pop
#endif

class HashSHA256 : public HashProvider
{
 public:
	std::string GenerateRaw(const std::string& data) CXX11_OVERRIDE
	{
		unsigned char bytes[SHA256_DIGEST_SIZE];
		sha256((unsigned char*)data.data(), data.length(),  bytes);
		return std::string((char*)bytes, SHA256_DIGEST_SIZE);
	}

	HashSHA256(Module* parent)
		: HashProvider(parent, "sha256", 32, 64)
	{
	}
};

class ModuleSHA256 : public Module
{
	HashSHA256 sha;
 public:
	ModuleSHA256() : sha(this)
	{
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Implements SHA-256 hashing", VF_VENDOR);
	}
};

MODULE_INIT(ModuleSHA256)
