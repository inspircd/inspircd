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

enum ProviderType
{
	PROV_MD5,
	PROV_SHA
};

/* Handle /MKPASSWD
 */
class cmd_mkpasswd : public command_t
{
	Module* Provider;
	Module* Sender;
	ProviderType Prov;
 public:
	cmd_mkpasswd (InspIRCd* Instance, Module* Sender, Module* Hasher, ProviderType P) : command_t(Instance,"MKPASSWD", 'o', 1), Provider(Hasher), Prov(P)
	{
		this->source = "m_oper_hash.so";
		syntax = "<any-text>";
	}

	CmdResult Handle (const char** parameters, int pcnt, userrec *user)
	{
		if (Prov == PROV_MD5)
		{
			MD5ResetRequest(Sender, Provider).Send();
			user->WriteServ("NOTICE %s :MD5 hashed password for %s is %s",user->nick,parameters[0], MD5SumRequest(Sender, Provider, parameters[0]).Send() );
		}
		else
		{
			SHA256ResetRequest(Sender, Provider).Send();
			user->WriteServ("NOTICE %s :SHA256 hashed password for %s is %s",user->nick,parameters[0], SHA256SumRequest(Sender, Provider, parameters[0]).Send() );
		}

		return CMD_SUCCESS;
	}
};

class ModuleOperHash : public Module
{
	
	cmd_mkpasswd* mycommand;
	Module* Provider;
	std::string providername;
	ProviderType ID;

 public:

	ModuleOperHash(InspIRCd* Me)
		: Module::Module(Me)
	{
		ConfigReader Conf(ServerInstance);
		providername = Conf.ReadValue("operhash","algorithm",0);

		if (providername.empty())
			providername = "md5";

		if (providername == "md5")
			ID = PROV_MD5;
		else
			ID = PROV_SHA;

		/* Try to find the md5 service provider, bail if it can't be found */
		Provider = ServerInstance->FindModule(std::string("m_") + providername + ".so");
		if (!Provider)
			throw ModuleException(std::string("Can't find m_") + providername + ".so. Please load m_" + providername + ".so before m_oper_hash.so.");

		mycommand = new cmd_mkpasswd(ServerInstance, this, Provider, ID);
		ServerInstance->AddCommand(mycommand);
	}
	
	virtual ~ModuleOperHash()
	{
	}

	void Implements(char* List)
	{
		List[I_OnOperCompare] = 1;
	}

	virtual int OnOperCompare(const std::string &data, const std::string &input)
	{
		/* always always reset first */
		if (ID == PROV_MD5)
		{
			MD5ResetRequest(this, Provider).Send();
			if (data.length() == 32) // if its 32 chars long, try it as an md5
			{
				/* Does it match the md5 sum? */
				if (!strcasecmp(data.c_str(), MD5SumRequest(this, Provider, input.c_str()).Send()))
				{
					return 1;
				}
				else return 0;
			}
		}
		else
		{
			SHA256ResetRequest(this, Provider).Send();
			if (data.length() == SHA256_BLOCK_SIZE)
			{
				if (!strcasecmp(data.c_str(), SHA256SumRequest(this, Provider, input.c_str()).Send()))
				{
					return 1;
				}
				else return 0;
			}
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
