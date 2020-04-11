/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2013, 2017-2018 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2010 Craig Edwards <brain@inspircd.org>
 *   Copyright (C) 2009 Uli Schlachter <psychon@inspircd.org>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
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

#ifdef __GNUC__
# pragma GCC diagnostic push
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
	std::string GenerateRaw(const std::string& data) override
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
 private:
	HashSHA256 sha;

 public:
	ModuleSHA256()
		: Module(VF_VENDOR, "Allows other modules to generate SHA-256 hashes.")
		, sha(this)
	{
	}
};

MODULE_INIT(ModuleSHA256)
