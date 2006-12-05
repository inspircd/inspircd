/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *           	  <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/* $ModDesc: Allows for hashed oper passwords */
/* $ModDep: m_md5.h m_sha256.h */

using namespace std;

#include "inspircd_config.h"
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "inspircd.h"

#include "m_md5.h"
#include "m_sha256.h"

enum ProviderTypes
{
	PROV_MD5 = 1,
	PROV_SHA = 2
};

/* Handle /MKPASSWD
 */
class cmd_mkpasswd : public command_t
{
	Module* MD5Provider;
	Module* SHAProvider;
	Module* Sender;
	int Prov;
 public:
	cmd_mkpasswd (InspIRCd* Instance, Module* Sender, Module* MD5Hasher, Module* SHAHasher, int P)
		: command_t(Instance,"MKPASSWD", 'o', 2), MD5Provider(MD5Hasher), SHAProvider(SHAHasher), Prov(P)
	{
		this->source = "m_oper_hash.so";
		syntax = "<hashtype> <any-text>";
	}

	CmdResult Handle (const char** parameters, int pcnt, userrec *user)
	{
		if (!strcasecmp(parameters[0], "md5"))
		{
			if ((Prov & PROV_MD5) > 0)
			{
				MD5ResetRequest(Sender, MD5Provider).Send();
				user->WriteServ("NOTICE %s :MD5 hashed password for %s is %s",user->nick, parameters[1], MD5SumRequest(Sender, MD5Provider, parameters[1]).Send() );
			}
			else
			{
				user->WriteServ("NOTICE %s :MD5 hashing is not available (m_md5.so not loaded)");
			}
		}
		else if (!strcasecmp(parameters[0], "sha256"))
		{
			if ((Prov & PROV_SHA) > 0)
			{
				SHA256ResetRequest(Sender, SHAProvider).Send();
				user->WriteServ("NOTICE %s :SHA256 hashed password for %s is %s",user->nick, parameters[1], SHA256SumRequest(Sender, SHAProvider, parameters[1]).Send() );
			}
			else
			{
				user->WriteServ("NOTICE %s :SHA256 hashing is not available (m_sha256.so not loaded)");
			}
		}
		else
		{
			user->WriteServ("NOTICE %s :Unknown hash type, valid hash types are:%s%s", (Prov & PROV_MD5) > 0 ? " MD5" : "", (Prov & PROV_SHA) > 0 ? " SHA256" : "");
		}

		/* NOTE: Don't propogate this across the network!
		 * We dont want plaintext passes going all over the place...
		 * To make sure it goes nowhere, return CMD_FAILURE!
		 */
		return CMD_FAILURE;
	}
};

class ModuleOperHash : public Module
{
	
	cmd_mkpasswd* mycommand;
	Module* MD5Provider;
	Module* SHAProvider;
	std::string providername;
	int ID;
	ConfigReader* Conf;

 public:

	ModuleOperHash(InspIRCd* Me)
		: Module::Module(Me)
	{
		Conf = NULL;
		OnRehash("");

		/* Try to find the md5 service provider, bail if it can't be found */
		MD5Provider = ServerInstance->FindModule("m_md5.so");
		if (MD5Provider)
			ID |= PROV_MD5;

		SHAProvider = ServerInstance->FindModule("m_sha256.so");
		if (SHAProvider)
			ID |= PROV_SHA;

		mycommand = new cmd_mkpasswd(ServerInstance, this, MD5Provider, SHAProvider, ID);
		ServerInstance->AddCommand(mycommand);
	}
	
	virtual ~ModuleOperHash()
	{
	}

	void Implements(char* List)
	{
		List[I_OnRehash] = List[I_OnOperCompare] = 1;
	}

	virtual void OnRehash(const std::string &parameter)
	{
		if (Conf)
			delete Conf;

		Conf = new ConfigReader(ServerInstance);
	}

	virtual int OnOperCompare(const std::string &data, const std::string &input, int tagnumber)
	{
		std::string hashtype = Conf->ReadValue("oper", "hash", tagnumber);
		if ((hashtype == "sha256") && (data.length() == SHA256_BLOCK_SIZE) && ((ID & PROV_SHA) > 0))
		{
			SHA256ResetRequest(this, SHAProvider).Send();
			if (!strcasecmp(data.c_str(), SHA256SumRequest(this, SHAProvider, input.c_str()).Send()))
				return 1;
			else return -1;
		}
		else if ((hashtype == "md5") && (data.length() == 32) && ((ID & PROV_MD5) > 0))
		{
			MD5ResetRequest(this, MD5Provider).Send();
			if (!strcasecmp(data.c_str(), MD5SumRequest(this, MD5Provider, input.c_str()).Send()))
				return 1;
			else return -1;
		}
		return 0;
	}
	
	virtual Version GetVersion()
	{
		return Version(1,1,0,1,VF_VENDOR,API_VERSION);
	}
};


class ModuleOperHashFactory : public ModuleFactory
{
 public:
	ModuleOperHashFactory()
	{
	}
	
	~ModuleOperHashFactory()
	{
	}
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleOperHash(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleOperHashFactory;
}
