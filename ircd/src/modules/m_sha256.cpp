/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2013, 2017-2018, 2021 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2008 Robin Burchell <robin+git@viroteck.net>
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

// Fix a collision between the Haiku uint64 typedef and the
// one from the sha2 library.
#ifdef __HAIKU__
# define uint64 sha2_uint64
#endif

#include <sha256.h>

#ifdef __HAIKU__
# undef uint64
#endif

#ifdef __GNUC__
# pragma GCC diagnostic pop
#endif

#include "inspircd.h"
#include "modules/hash.h"

class HashSHA256 : public HashProvider {
  public:
    std::string GenerateRaw(const std::string& data) CXX11_OVERRIDE {
        SHA256 sha256;
        return std::string(sha256(data.data()), 32);
    }

    HashSHA256(Module* parent)
        : HashProvider(parent, "sha256", 32, 64) {
    }
};

class ModuleSHA256 : public Module {
    HashSHA256 sha;
  public:
    ModuleSHA256() : sha(this) {
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Allows other modules to generate SHA-256 hashes.", VF_VENDOR);
    }
};

MODULE_INIT(ModuleSHA256)
