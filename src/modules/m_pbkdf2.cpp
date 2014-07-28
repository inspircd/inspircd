/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
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


#include "inspircd.h"
#include "modules/hash.h"

// Format:
// Iterations:B64(Hash):B64(Salt)
// E.g.
// 10200:AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA:BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB
class PBKDF2Hash
{
public:
	unsigned int iterations;
	unsigned int length;
	std::string salt;
	std::string hash;

	PBKDF2Hash(unsigned int itr = 0, unsigned int dkl = 0, const std::string& slt = "", const std::string& hsh = "")
		: iterations(itr), length(dkl), salt(slt), hash(hsh)
	{
	}

	PBKDF2Hash(const std::string& data)
	{
		irc::sepstream ss(data, ':');
		std::string tok;

		ss.GetToken(tok);
		this->iterations = ConvToInt(tok);

		ss.GetToken(tok);
		this->hash = Base64ToBin(tok);

		ss.GetToken(tok);
		this->salt = Base64ToBin(tok);

		this->length = this->hash.length();
	}

	std::string ToString()
	{
		if (!IsValid())
			return "";
		return ConvToStr(this->iterations) + ":" + BinToBase64(this->hash) + ":" + BinToBase64(this->salt);
	}

	bool IsValid()
	{
		if (!this->iterations || !this->length || this->salt.empty() || this->hash.empty())
			return false;
		return true;
	}
};

class PBKDF2Provider : public HashProvider
{
 public:
	HashProvider* provider;
	unsigned int iterations;
	unsigned int dkey_length;

	std::string PBKDF2(const std::string& pass, const std::string& salt, unsigned int itr = 0, unsigned int dkl = 0)
	{
		if (!iterations)
			iterations = itr;
		if (!dkey_length)
			dkey_length = dkl;

		size_t blocks = std::ceil((double)dkey_length / provider->out_size);

		std::string output;
		for (size_t block = 1; block <= blocks; block++)
		{
			char salt_data[4];
			for (size_t i = 0; i < sizeof(salt_data); i++)
				salt_data[i] = block >> (24 - i * 8) & 0x0F;

			std::string salt_block(salt_data, 4);
			salt_block = salt + salt_block;

			std::string blockdata;
			std::string lasthash = blockdata = provider->HMAC(pass, salt_block);
			for (size_t iter = 1; iter < iterations; iter++)
			{
				std::string tmphash = provider->HMAC(pass, lasthash);
				for (size_t i = 0; i < provider->out_size; i++)
					blockdata[i] ^= tmphash[i];

				lasthash = tmphash;
			}
			output += blockdata;
		}

		output = output.substr(0, dkey_length);
		return output;
	}

	std::string Generate(const std::string& data, const HashProvider::HashType type)
	{
		PBKDF2Hash hs(
			this->iterations,
			this->dkey_length,
			ServerInstance->GenRandomStr(dkey_length, false)
		);
		hs.hash = PBKDF2(data, hs.salt);

		const std::string ret = hs.ToString();
		return ret;
	}

	bool Compare(const std::string& string, const std::string& hash) CXX11_OVERRIDE
	{
		PBKDF2Hash hs(hash);
		if (!hs.IsValid())
			return false;

		std::string cmp = PBKDF2(string, hs.salt, hs.iterations, hs.length);
		return cmp == hs.hash;
	}

	std::string RAW(const std::string& raw)
	{
		return raw;
	}

	PBKDF2Provider(Module* mod, HashProvider* hp) :
		HashProvider(mod, "pbkdf2-hmac-" + hp->name.substr(hp->name.find('/') + 1)), provider(hp)
	{
		DisableAutoRegister();
	}
};

class ModulePBKDF2 : public Module
{
	std::vector<PBKDF2Provider*> provider_info;

	void GetConfig()
	{
		// First set the common values
		ConfigTag* tag = ServerInstance->Config->ConfValue("pbkdf2");
		unsigned int global_rounds = tag->getInt("iterations", 12288, 1);
		unsigned int global_dkey_length = tag->getInt("length", 32, 1, 1024);
		for (std::vector<PBKDF2Provider*>::iterator it = provider_info.begin(); it != provider_info.end(); ++it)
		{
			PBKDF2Provider* pi = *it;
			pi->iterations = global_rounds;
			pi->dkey_length = global_dkey_length;
		}

		// Then the specific values
		ConfigTagList tags = ServerInstance->Config->ConfTags("pbkdf2prov");
		for (ConfigIter i = tags.first; i != tags.second; ++i)
		{
			std::string hash_name = i->second->getString("hash");
			for (std::vector<PBKDF2Provider*>::iterator it = provider_info.begin(); it != provider_info.end(); ++it)
			{
				PBKDF2Provider* pi = *it;
				if (pi->provider->name != "hash/" + hash_name)
					continue;

				pi->iterations = i->second->getInt("iterations", global_rounds, 1);
				pi->dkey_length = i->second->getInt("length", global_dkey_length, 1, 1024);
			}
		}
	}

 public:
	~ModulePBKDF2()
	{
		stdalgo::delete_all(provider_info);
	}

	void Prioritize() CXX11_OVERRIDE
	{
		OnLoadModule(NULL);
	}

	void OnLoadModule(Module* mod) CXX11_OVERRIDE
	{
		bool newProv = false;
		// As the module doesn't tell us what ServiceProviders it has, let's iterate all (yay ...) the ServiceProviders
		// Good thing people don't run loading and unloading those all the time
		for (std::multimap<std::string, ServiceProvider*>::iterator i = ServerInstance->Modules->DataProviders.begin(); i != ServerInstance->Modules->DataProviders.end(); ++i)
		{
			// Does the service belong to the new mod?
			// In the case this is our first run (mod == NULL, continue anyway)
			if (mod && i->second->creator != mod)
				continue;

			// Checking if it's a hash provider
			if (i->second->name.compare(0, 5, "hash/"))
				continue;

			HashProvider* hp = static_cast<HashProvider*>(i->second);

			if (hp->IsKDF())
				continue;

			bool has_prov = false;
			for (std::vector<PBKDF2Provider*>::const_iterator it = provider_info.begin(); it != provider_info.end(); ++it)
			{
				if ((*it)->provider == hp)
				{
					has_prov = true;
					break;
				}
			}
			if (has_prov)
				continue;

			newProv = true;

			PBKDF2Provider* prov = new PBKDF2Provider(this, hp);
			provider_info.push_back(prov);
			ServerInstance->Modules->AddService(*prov);
		}

		if (newProv)
			GetConfig();
	}

	void OnUnloadModule(Module* mod) CXX11_OVERRIDE
	{
		for (std::vector<PBKDF2Provider*>::iterator it = provider_info.begin(); it != provider_info.end(); )
		{
			if ((*it)->provider->creator != mod)
			{
				++it;
				continue;
			}

			ServerInstance->Modules->DelService(**it);
			delete *it;
			it = provider_info.erase(it);
		}
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		GetConfig();
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Implements PBKDF2 hashing", VF_VENDOR);
	}
};

MODULE_INIT(ModulePBKDF2)
