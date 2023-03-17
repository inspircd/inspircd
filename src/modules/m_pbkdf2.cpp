/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2018, 2020 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2018 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2014, 2020 Daniel Vassdal <shutter@canternet.org>
 *   Copyright (C) 2014, 2016 Attila Molnar <attilamolnar@hush.com>
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


#include "inspircd.h"
#include "modules/hash.h"

// Format:
// Iterations:B64(Hash):B64(Salt)
// E.g.
// 10200:AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA:BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB
class PBKDF2Hash {
  public:
    unsigned int iterations;
    unsigned int length;
    std::string salt;
    std::string hash;

    PBKDF2Hash(unsigned int itr, unsigned int dkl, const std::string& slt,
               const std::string& hsh = "")
        : iterations(itr), length(dkl), salt(slt), hash(hsh) {
    }

    PBKDF2Hash(const std::string& data) {
        irc::sepstream ss(data, ':');
        std::string tok;

        ss.GetToken(tok);
        this->iterations = ConvToNum<unsigned int>(tok);

        ss.GetToken(tok);
        this->hash = Base64ToBin(tok);

        ss.GetToken(tok);
        this->salt = Base64ToBin(tok);

        this->length = this->hash.length();
    }

    std::string ToString() {
        if (!IsValid()) {
            return "";
        }
        return ConvToStr(this->iterations) + ":" + BinToBase64(this->hash) + ":" +
               BinToBase64(this->salt);
    }

    bool IsValid() {
        if (!this->iterations || !this->length || this->salt.empty()
                || this->hash.empty()) {
            return false;
        }
        return true;
    }
};

class PBKDF2Provider : public HashProvider {
  public:
    HashProvider* provider;
    unsigned int iterations;
    unsigned int dkey_length;

    std::string PBKDF2(const std::string& pass, const std::string& salt,
                       unsigned int itr = 0, unsigned int dkl = 0) {
        size_t blocks = std::ceil((double)dkl / provider->out_size);

        std::string output;
        std::string tmphash;
        std::string salt_block = salt;
        for (size_t block = 1; block <= blocks; block++) {
            char salt_data[4];
            for (size_t i = 0; i < sizeof(salt_data); i++) {
                salt_data[i] = block >> (24 - i * 8) & 0x0F;
            }

            salt_block.erase(salt.length());
            salt_block.append(salt_data, sizeof(salt_data));

            std::string blockdata = provider->hmac(pass, salt_block);
            std::string lasthash = blockdata;
            for (size_t iter = 1; iter < itr; iter++) {
                tmphash = provider->hmac(pass, lasthash);
                for (size_t i = 0; i < provider->out_size; i++) {
                    blockdata[i] ^= tmphash[i];
                }

                lasthash.swap(tmphash);
            }
            output += blockdata;
        }

        output.erase(dkl);
        return output;
    }

    std::string GenerateRaw(const std::string& data) CXX11_OVERRIDE {
        PBKDF2Hash hs(this->iterations, this->dkey_length, ServerInstance->GenRandomStr(dkey_length, false));
        hs.hash = PBKDF2(data, hs.salt, this->iterations, this->dkey_length);
        return hs.ToString();
    }

    bool Compare(const std::string& input, const std::string& hash) CXX11_OVERRIDE {
        PBKDF2Hash hs(hash);
        if (!hs.IsValid()) {
            return false;
        }

        std::string cmp = PBKDF2(input, hs.salt, hs.iterations, hs.length);
        return InspIRCd::TimingSafeCompare(cmp, hs.hash);
    }

    std::string ToPrintable(const std::string& raw) CXX11_OVERRIDE {
        return raw;
    }

    PBKDF2Provider(Module* mod, HashProvider* hp)
        : HashProvider(mod, "pbkdf2-hmac-" + hp->name.substr(hp->name.find('/') + 1))
        , provider(hp) {
        DisableAutoRegister();
    }
};

struct ProviderConfig {
    unsigned long dkey_length;
    unsigned long iterations;
};

typedef std::map<std::string, ProviderConfig> ProviderConfigMap;

class ModulePBKDF2 : public Module {
    std::vector<PBKDF2Provider*> providers;
    ProviderConfig globalconfig;
    ProviderConfigMap providerconfigs;

    ProviderConfig GetConfigForProvider(const std::string& name) const {
        ProviderConfigMap::const_iterator it = providerconfigs.find(name);
        if (it == providerconfigs.end()) {
            return globalconfig;
        }

        return it->second;
    }

    void ConfigureProviders() {
        for (std::vector<PBKDF2Provider*>::iterator i = providers.begin();
                i != providers.end(); ++i) {
            PBKDF2Provider* pi = *i;
            ProviderConfig config = GetConfigForProvider(pi->name);
            pi->iterations = config.iterations;
            pi->dkey_length = config.dkey_length;
        }
    }

    void GetConfig() {
        // First set the common values
        ConfigTag* tag = ServerInstance->Config->ConfValue("pbkdf2");
        ProviderConfig newglobal;
        newglobal.iterations = tag->getUInt("iterations", 12288, 1);
        newglobal.dkey_length = tag->getUInt("length", 32, 1, 1024);

        // Then the specific values
        ProviderConfigMap newconfigs;
        ConfigTagList tags = ServerInstance->Config->ConfTags("pbkdf2prov");
        for (ConfigIter i = tags.first; i != tags.second; ++i) {
            tag = i->second;
            std::string hash_name = "hash/" + tag->getString("hash");
            ProviderConfig& config = newconfigs[hash_name];

            config.iterations = tag->getUInt("iterations", newglobal.iterations, 1);
            config.dkey_length = tag->getUInt("length", newglobal.dkey_length, 1, 1024);
        }

        // Config is valid, apply it
        providerconfigs.swap(newconfigs);
        std::swap(globalconfig, newglobal);
        ConfigureProviders();
    }

  public:
    ~ModulePBKDF2() {
        stdalgo::delete_all(providers);
    }

    void init() CXX11_OVERRIDE {
        // Let ourself know about any existing services.
        const ModuleManager::DataProviderMap& dataproviders = ServerInstance->Modules->DataProviders;
        for (ModuleManager::DataProviderMap::const_iterator it = dataproviders.begin(); it != dataproviders.end(); ++it) {
            OnServiceAdd(*it->second);
        }
    }

    void OnServiceAdd(ServiceProvider& provider) CXX11_OVERRIDE {
        // Check if it's a hash provider
        if (provider.name.compare(0, 5, "hash/")) {
            return;
        }

        HashProvider* hp = static_cast<HashProvider*>(&provider);
        if (hp->IsKDF()) {
            return;
        }

        PBKDF2Provider* prov = new PBKDF2Provider(this, hp);
        providers.push_back(prov);
        ServerInstance->Modules.AddService(*prov);

        ConfigureProviders();
    }

    void OnServiceDel(ServiceProvider& prov) CXX11_OVERRIDE {
        for (std::vector<PBKDF2Provider*>::iterator i = providers.begin(); i != providers.end(); ++i) {
            PBKDF2Provider* item = *i;
            if (item->provider != &prov) {
                continue;
            }

            ServerInstance->Modules->DelService(*item);
            delete item;
            providers.erase(i);
            break;
        }
    }

    void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE {
        GetConfig();
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Allows other modules to generate PBKDF2 hashes.", VF_VENDOR);
    }
};

MODULE_INIT(ModulePBKDF2)
