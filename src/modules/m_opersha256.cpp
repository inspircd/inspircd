/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *                <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/* m_opersha256 - Originally written by Special <john@yarbbles.com>
 * Updated December 2006 by Craig Edwards
 */

/* $ModDesc: Allows for SHA-256 encrypted oper passwords */
/* $ModDep: m_sha256.h */

#include "inspircd_config.h"
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "inspircd.h"

#include "m_sha256.h"

class cmd_mksha256 : public command_t
{
	Module* Source;
	Module* SHA256Provider;
 public:
	cmd_mksha256 (InspIRCd* Instance, Module* Src, Module* SHA256) : command_t(Instance,"MKSHA256", 'o', 1), Source(Src), SHA256Provider(SHA256)
	{
		this->source = "m_opersha256.so";
		syntax = "<any-text>";
	}

	CmdResult Handle(const char** parameters, int pcnt, userrec *user)
	{
		SHA256ResetRequest(Source, SHA256Provider).Send();
		user->WriteServ("NOTICE %s :SHA256 hashed password for %s is %s", user->nick, parameters[0], SHA256SumRequest(Source, SHA256Provider, parameters[0]).Send() );
		return CMD_SUCCESS;
	}
};

class ModuleOperSHA256 : public Module
{
	cmd_mksha256 *mksha256cmd;
	Module* SHA256Provider;
public:

	ModuleOperSHA256(InspIRCd* Me) : Module::Module(Me)
	{
		SHA256Provider = ServerInstance->FindModule("m_sha256.so");
		if (!SHA256Provider)
			throw ModuleException("Can't find m_sha256.so. Please load m_sha256.so before m_opersha256.so.");

		mksha256cmd = new cmd_mksha256(ServerInstance, this, SHA256Provider);
		ServerInstance->AddCommand(mksha256cmd);
	}

	virtual ~ModuleOperSHA256()
	{
	}

	void Implements(char *List)
	{
		List[I_OnOperCompare] = 1;
	}

	virtual int OnOperCompare(const std::string &data, const std::string &input)
	{
		SHA256ResetRequest(this, SHA256Provider).Send();
		if (data.length() == SHA256_BLOCK_SIZE) // If the data is as long as a hex sha256 hash, try it as that
		{
			if (!strcasecmp(data.c_str(), SHA256SumRequest(this, SHA256Provider, input.c_str()).Send() ))
				return 1;
			else
				return -1;
		}
		return 0;
	}

	virtual Version GetVersion()
	{
		return Version(1, 1, 0, 1, VF_VENDOR, API_VERSION);
	}
};


class ModuleOperSHA256Factory : public ModuleFactory
{
public:
	ModuleOperSHA256Factory()
	{
	}

	~ModuleOperSHA256Factory()
	{
	}

	virtual Module *CreateModule(InspIRCd* Me)
	{
		return new ModuleOperSHA256(Me);
	}

};

extern "C" void * init_module( void )
{
	return new ModuleOperSHA256Factory;
}
