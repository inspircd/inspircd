/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
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


/* $ModDesc: Allows for hashed oper passwords */

#include "inspircd.h"
#include "hash.h"

/* Handle /MKPASSWD
 */
class CommandMkpasswd : public Command
{
 public:
	CommandMkpasswd(Module* Creator) : Command(Creator, "MKPASSWD", 2)
	{
		syntax = "<hashtype> <any-text>";
		Penalty = 5;
	}

	void MakeHash(User* user, const std::string& algo, const std::string& stuff)
	{
		if (algo.substr(0,5) == "hmac-")
		{
			std::string type = algo.substr(5);
			HashProvider* hp = ServerInstance->Modules->FindDataService<HashProvider>("hash/" + type);
			if (!hp)
			{
				user->WriteServ("NOTICE %s :Unknown hash type", user->nick.c_str());
				return;
			}
			std::string salt = ServerInstance->GenRandomStr(6, false);
			std::string target = hp->hmac(salt, stuff);
			std::string str = BinToBase64(salt) + "$" + BinToBase64(target, NULL, 0);

			user->WriteServ("NOTICE %s :%s hashed password for %s is %s",
				user->nick.c_str(), algo.c_str(), stuff.c_str(), str.c_str());
			return;
		}
		HashProvider* hp = ServerInstance->Modules->FindDataService<HashProvider>("hash/" + algo);
		if (hp)
		{
			/* Now attempt to generate a hash */
			std::string hexsum = hp->hexsum(stuff);
			user->WriteServ("NOTICE %s :%s hashed password for %s is %s",
				user->nick.c_str(), algo.c_str(), stuff.c_str(), hexsum.c_str());
		}
		else
		{
			user->WriteServ("NOTICE %s :Unknown hash type", user->nick.c_str());
		}
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		MakeHash(user, parameters[0], parameters[1]);

		return CMD_SUCCESS;
	}
};

class ModuleOperHash : public Module
{
	CommandMkpasswd cmd;
 public:

	ModuleOperHash() : cmd(this)
	{
	}

	void init()
	{
		/* Read the config file first */
		OnRehash(NULL);

		ServerInstance->Modules->AddService(cmd);
		Implementation eventlist[] = { I_OnPassCompare };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	virtual ModResult OnPassCompare(Extensible* ex, const std::string &data, const std::string &input, const std::string &hashtype)
	{
		if (hashtype.substr(0,5) == "hmac-")
		{
			std::string type = hashtype.substr(5);
			HashProvider* hp = ServerInstance->Modules->FindDataService<HashProvider>("hash/" + type);
			if (!hp)
				return MOD_RES_PASSTHRU;
			// this is a valid hash, from here on we either accept or deny
			std::string::size_type sep = data.find('$');
			if (sep == std::string::npos)
				return MOD_RES_DENY;
			std::string salt = Base64ToBin(data.substr(0, sep));
			std::string target = Base64ToBin(data.substr(sep + 1));

			if (target == hp->hmac(salt, input))
				return MOD_RES_ALLOW;
			else
				return MOD_RES_DENY;
		}

		HashProvider* hp = ServerInstance->Modules->FindDataService<HashProvider>("hash/" + hashtype);

		/* Is this a valid hash name? */
		if (hp)
		{
			/* Compare the hash in the config to the generated hash */
			if (data == hp->hexsum(input))
				return MOD_RES_ALLOW;
			else
				/* No match, and must be hashed, forbid */
				return MOD_RES_DENY;
		}

		/* Not a hash, fall through to strcmp in core */
		return MOD_RES_PASSTHRU;
	}

	virtual Version GetVersion()
	{
		return Version("Allows for hashed oper passwords",VF_VENDOR);
	}
};

MODULE_INIT(ModuleOperHash)
