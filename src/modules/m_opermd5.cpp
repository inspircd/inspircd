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

/* $ModDesc: Allows for MD5 encrypted oper passwords */
/* $ModDep: m_md5.h */

using namespace std;

#include "inspircd_config.h"
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "inspircd.h"

#include "m_md5.h"

/* Handle /MKPASSWD
 */
class cmd_mkpasswd : public command_t
{
	Module* MD5Provider;
	Module* Sender;
 public:
	cmd_mkpasswd (InspIRCd* Instance, Module* Sender, Module* MD5) : command_t(Instance,"MKPASSWD", 'o', 1), MD5Provider(MD5)
	{
		this->source = "m_opermd5.so";
		syntax = "<any-text>";
	}

	CmdResult Handle (const char** parameters, int pcnt, userrec *user)
	{
		MD5ResetRequest(Sender, MD5Provider).Send();
		user->WriteServ("NOTICE %s :MD5 hashed password for %s is %s",user->nick,parameters[0], MD5SumRequest(Sender, MD5Provider, parameters[0]).Send() );
		return CMD_SUCCESS;
	}
};

class ModuleOperMD5 : public Module
{
	
	cmd_mkpasswd* mycommand;
	Module* MD5Provider;

 public:

	ModuleOperMD5(InspIRCd* Me)
		: Module::Module(Me)
	{
		MD5Provider = ServerInstance->FindModule("m_md5.so");
		if (!MD5Provider)
			throw ModuleException("Can't find m_md5.so. Please load m_md5.so before m_opermd5.so.");

		mycommand = new cmd_mkpasswd(ServerInstance, this, MD5Provider);
		ServerInstance->AddCommand(mycommand);
	}
	
	virtual ~ModuleOperMD5()
	{
	}

	void Implements(char* List)
	{
		List[I_OnOperCompare] = 1;
	}

	virtual int OnOperCompare(const std::string &data, const std::string &input)
	{
		MD5ResetRequest(this, MD5Provider).Send();
		if (data.length() == 32) // if its 32 chars long, try it as an md5
		{
			if (!strcasecmp(data.c_str(), MD5SumRequest(this, MD5Provider, input.c_str()).Send()))
			{
				return 1;
			}
			else return 0;
		}
		return 0;
	}
	
	virtual Version GetVersion()
	{
		return Version(1,1,0,1,VF_VENDOR,API_VERSION);
	}
};


class ModuleOperMD5Factory : public ModuleFactory
{
 public:
	ModuleOperMD5Factory()
	{
	}
	
	~ModuleOperMD5Factory()
	{
	}
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleOperMD5(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleOperMD5Factory;
}
