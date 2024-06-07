/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2019-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Craig Edwards <brain@inspircd.org>
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


#include <md5/md5.c>
#undef F
#ifdef _WIN32
# undef OUT
#endif

#include "inspircd.h"
#include "modules/hash.h"

class MD5Provider final
	: public HashProvider
{
public:
	std::string GenerateRaw(const std::string& data) override
	{
		MD5_CTX context;
		MD5_Init(&context);
		MD5_Update(&context, reinterpret_cast<const unsigned char*>(data.data()), data.length());

		std::vector<unsigned char> bytes(16);
		MD5_Final(bytes.data(), &context);
		return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
	}

	MD5Provider(Module* parent)
		: HashProvider(parent, "md5", 16, 64)
	{
	}
};

class ModuleMD5 final
	: public Module
{
private:
	MD5Provider md5;

public:
	ModuleMD5()
		: Module(VF_VENDOR, "Allows other modules to generate MD5 hashes.")
		, md5(this)
	{
	}
};

MODULE_INIT(ModuleMD5)
