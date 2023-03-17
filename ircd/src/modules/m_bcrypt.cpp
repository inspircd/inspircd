/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2017-2018, 2020 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2014 Daniel Vassdal <shutter@canternet.org>
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

/// $CompilerFlags: -Ivendor_directory("bcrypt")


#include "inspircd.h"
#include "modules/hash.h"

#include <crypt.h>

class BCryptProvider : public HashProvider {
  private:
    std::string Salt() {
        char entropy[16];
        for (unsigned int i = 0; i < sizeof(entropy); ++i) {
            entropy[i] = ServerInstance->GenRandomInt(0xFF);
        }

        char salt[32];
        if (!crypt_gensalt_rn("$2a$", rounds, entropy, sizeof(entropy), salt,
                              sizeof(salt))) {
            throw ModuleException("Could not generate salt - this should never happen");
        }

        return salt;
    }

  public:
    unsigned int rounds;

    std::string Generate(const std::string& data, const std::string& salt) {
        char hash[64];
        crypt_rn(data.c_str(), salt.c_str(), hash, sizeof(hash));
        return hash;
    }

    std::string GenerateRaw(const std::string& data) CXX11_OVERRIDE {
        return Generate(data, Salt());
    }

    bool Compare(const std::string& input,
                 const std::string& hash)  CXX11_OVERRIDE {
        return InspIRCd::TimingSafeCompare(Generate(input, hash), hash);
    }

    std::string ToPrintable(const std::string& raw) CXX11_OVERRIDE {
        return raw;
    }

    BCryptProvider(Module* parent)
        : HashProvider(parent, "bcrypt", 60)
        , rounds(10) {
    }
};

class ModuleBCrypt : public Module {
    BCryptProvider bcrypt;

  public:
    ModuleBCrypt() : bcrypt(this) {
    }

    void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE {
        ConfigTag* conf = ServerInstance->Config->ConfValue("bcrypt");
        bcrypt.rounds = conf->getUInt("rounds", 10, 1);
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Allows other modules to generate bcrypt hashes.", VF_VENDOR);
    }
};

MODULE_INIT(ModuleBCrypt)
