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


#include "inspircd.h"
#include "modules/hash.h"

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
		if (!algo.compare(0, 5, "hmac-", 5))
		{
			std::string type(algo, 5);
			HashProvider* hp = ServerInstance->Modules->FindDataService<HashProvider>("hash/" + type);
			if (!hp)
			{
				user->WriteNotice("Unknown hash type");
				return;
			}

			if (hp->IsKDF())
			{
				user->WriteNotice(type + " does not support HMAC");
				return;
			}

			std::string salt = ServerInstance->GenRandomStr(hp->out_size, false);
			std::string target = hp->hmac(salt, stuff);
			std::string str = BinToBase64(salt) + "$" + BinToBase64(target, NULL, 0);

			user->WriteNotice(algo + " hashed password for " + stuff + " is " + str);
			return;
		}
		HashProvider* hp = ServerInstance->Modules->FindDataService<HashProvider>("hash/" + algo);
		if (hp)
		{
			/* Now attempt to generate a hash */
			std::string hexsum = hp->Generate(stuff);
			user->WriteNotice(algo + " hashed password for " + stuff + " is " + hexsum);
		}
		else
		{
			user->WriteNotice("Unknown hash type");
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

	ModResult OnPassCompare(Extensible* ex, const std::string &data, const std::string &input, const std::string &hashtype) CXX11_OVERRIDE
	{
		if (!hashtype.compare(0, 5, "hmac-", 5))
		{
			std::string type(hashtype, 5);
			HashProvider* hp = ServerInstance->Modules->FindDataService<HashProvider>("hash/" + type);
			if (!hp)
				return MOD_RES_PASSTHRU;

			if (hp->IsKDF())
			{
				ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "Tried to use HMAC with %s, which does not support HMAC", type.c_str());
				return MOD_RES_DENY;
			}

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
			if (hp->Compare(input, data))
				return MOD_RES_ALLOW;
			else
				/* No match, and must be hashed, forbid */
				return MOD_RES_DENY;
		}

		// We don't handle this type, let other mods or the core decide
		return MOD_RES_PASSTHRU;
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Allows for hashed oper passwords",VF_VENDOR);
	}
};

MODULE_INIT(ModuleOperHash)
