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

#include "inspircd_config.h"
#include "configreader.h"
#include "inspircd.h"
#include "users.h"
#include "channels.h"
#include "modules.h"

#include "m_md5.h"

/* $ModDesc: Provides masking of user hostnames */
/* $ModDep: m_md5.h */

/* Used to vary the output a little more depending on the cloak keys */
static const char* xtab[] = {"F92E45D871BCA630", "A1B9D80C72E653F4", "1ABC078934DEF562", "ABCDEF5678901234"};

/** Handles user mode +x
 */
class CloakUser : public ModeHandler
{
	
	std::string prefix;
	unsigned int key1;
	unsigned int key2;
	unsigned int key3;
	unsigned int key4;
	Module* Sender;
	Module* MD5Provider;
	
 public:
	CloakUser(InspIRCd* Instance, Module* Source, Module* MD5) : ModeHandler(Instance, 'x', 0, 0, false, MODETYPE_USER, false), Sender(Source), MD5Provider(MD5)
	{
	}

	ModeAction OnModeChange(userrec* source, userrec* dest, chanrec* channel, std::string &parameter, bool adding)
	{
		/* Only opers can change other users modes */
		if ((source != dest) && (!*source->oper))
			return MODEACTION_DENY;

		/* For remote clients, we dont take any action, we just allow it.
		 * The local server where they are will set their cloak instead.
		 */
		if (!IS_LOCAL(dest))
			return MODEACTION_ALLOW;

		if (adding)
		{
			if(!dest->IsModeSet('x'))
			{
				/* The mode is being turned on - so attempt to
				 * allocate the user a cloaked host using a non-reversible
				 * algorithm (its simple, but its non-reversible so the
				 * simplicity doesnt really matter). This algorithm
				 * will not work if the user has only one level of domain
				 * naming in their hostname (e.g. if they are on a lan or
				 * are connecting via localhost) -- this doesnt matter much.
				 */
			
				if (strchr(dest->host,'.') || strchr(dest->host,':'))
				{
					/* InspIRCd users have two hostnames; A displayed
					 * hostname which can be modified by modules (e.g.
					 * to create vhosts, implement chghost, etc) and a
					 * 'real' hostname which you shouldnt write to.
					 */

					unsigned int iv[] = { key1, key2, key3, key4 };
					char* n = strstr(dest->host,".");
					if (!n)
						n = strstr(dest->host,":");

					std::string a = n;

					std::string b;
					insp_inaddr testaddr;

					/** Reset the MD5 module, and send it our IV and hex table */
					MD5ResetRequest(Sender, MD5Provider).Send();
					MD5KeyRequest(Sender, MD5Provider, iv).Send();
					MD5HexRequest(Sender, MD5Provider, xtab[0]);

					/* Generate a cloak using specialized MD5 */
					std::string hostcloak = prefix + "-" + MD5SumRequest(Sender, MD5Provider, dest->host).Send() + a;

					/* Fix by brain - if the cloaked host is > the max length of a host (64 bytes
					 * according to the DNS RFC) then tough titty, they get cloaked as an IP. 
					 * Their ISP shouldnt go to town on subdomains, or they shouldnt have a kiddie
					 * vhost.
					 */

					if ((insp_aton(dest->host,&testaddr) < 1) && (hostcloak.length() <= 64))
					{
						// if they have a hostname, make something appropriate
						b = hostcloak;
					}
					else
					{
						b = ((b.find(':') == std::string::npos) ? Cloak4(dest->host) : Cloak6(dest->host));
					}
					dest->ChangeDisplayedHost(b.c_str());
				}
				
				dest->SetMode('x',true);
				return MODEACTION_ALLOW;
			}
		}
		else
  		{
			if (dest->IsModeSet('x'))
			{
  				/* User is removing the mode, so just restore their real host
  				 * and make it match the displayed one.
				 */
				dest->ChangeDisplayedHost(dest->host);
				dest->SetMode('x',false);
				return MODEACTION_ALLOW;
			}
		}

		return MODEACTION_DENY;
	}

	std::string Cloak4(const char* ip)
	{
		unsigned int iv[] = { key1, key2, key3, key4 };
		irc::sepstream seps(ip, '.');
		std::string ra1, ra2, ra3, ra4;
		std::string octet1 = seps.GetToken();
		std::string octet2 = seps.GetToken();
		std::string octet3 = seps.GetToken();
		std::string octet4 = seps.GetToken();
		int i1 = atoi(octet1.c_str());
		int i2 = atoi(octet2.c_str());
		int i3 = atoi(octet3.c_str());
		int i4 = atoi(octet4.c_str());

		octet4 = octet1 + "." + octet2 + "." + octet3 + "." + octet4;
		octet3 = octet1 + "." + octet2 + "." + octet3;
		octet2 = octet1 + "." + octet2;

		/* Reset the MD5 module and send it our IV */
		MD5ResetRequest(Sender, MD5Provider).Send();
		MD5KeyRequest(Sender, MD5Provider, iv).Send();

		/* Send the MD5 module a different hex table for each octet group's MD5 sum */
		MD5HexRequest(Sender, MD5Provider, xtab[(key1+i1) % 4]).Send();
		ra1 = std::string(MD5SumRequest(Sender, MD5Provider, octet1).Send()).substr(0,6);

		MD5HexRequest(Sender, MD5Provider, xtab[(key2+i2) % 4]).Send();
		ra2 = std::string(MD5SumRequest(Sender, MD5Provider, octet2).Send()).substr(0,6);

		MD5HexRequest(Sender, MD5Provider, xtab[(key3+i3) % 4]).Send();
		ra3 = std::string(MD5SumRequest(Sender, MD5Provider, octet3).Send()).substr(0,6);

		MD5HexRequest(Sender, MD5Provider, xtab[(key4+i4) % 4]).Send();
		ra4 = std::string(MD5SumRequest(Sender, MD5Provider, octet4).Send()).substr(0,6);

		/* Stick them all together */
		return std::string().append(ra1).append(".").append(ra2).append(".").append(ra3).append(".").append(ra4);
	}

	std::string Cloak6(const char* ip)
	{
		unsigned int iv[] = { key1, key2, key3, key4 };
		std::vector<std::string> hashies;
		std::string item = "";
		int rounds = 0;

		/* Reset the MD5 module and send it our IV */
		MD5ResetRequest(Sender, MD5Provider).Send();
		MD5KeyRequest(Sender, MD5Provider, iv).Send();

		for (const char* input = ip; *input; input++)
		{
			item += *input;
			if (item.length() > 5)
			{
				/* Send the MD5 module a different hex table for each octet group's MD5 sum */
				MD5HexRequest(Sender, MD5Provider, xtab[(key1+rounds) % 4]).Send();
				hashies.push_back(std::string(MD5SumRequest(Sender, MD5Provider, item).Send()).substr(0,10));
				item = "";
			}
			rounds++;
		}
		if (!item.empty())
		{
			/* Send the MD5 module a different hex table for each octet group's MD5 sum */
			MD5HexRequest(Sender, MD5Provider, xtab[(key1+rounds) % 4]).Send();
			hashies.push_back(std::string(MD5SumRequest(Sender, MD5Provider, item).Send()).substr(0,10));
			item = "";
		}
		/* Stick them all together */
		return irc::stringjoiner(":", hashies, 0, hashies.size() - 1).GetJoined();
	}
	
	void DoRehash()
	{
		ConfigReader Conf(ServerInstance);
		key1 = key2 = key3 = key4 = 0;
		key1 = Conf.ReadInteger("cloak","key1",0,true);
		key2 = Conf.ReadInteger("cloak","key2",0,true);
		key3 = Conf.ReadInteger("cloak","key3",0,true);
		key4 = Conf.ReadInteger("cloak","key4",0,true);
		prefix = Conf.ReadValue("cloak","prefix",0);

		if (prefix.empty())
			prefix = ServerInstance->Config->Network;

		if (!key1 && !key2 && !key3 && !key4)
		{
			ModuleException ex("You have not defined cloak keys for m_cloaking!!! THIS IS INSECURE AND SHOULD BE CHECKED!");
			throw (ex);
		}
	}
};


class ModuleCloaking : public Module
{
 private:
	
 	CloakUser* cu;
	Module* MD5Module;

 public:
	ModuleCloaking(InspIRCd* Me)
		: Module::Module(Me)
	{
		/* Attempt to locate the MD5 service provider, bail if we can't find it */
		MD5Module = ServerInstance->FindModule("m_md5.so");
		if (!MD5Module)
			throw ModuleException("Can't find m_md5.so. Please load m_md5.so before m_cloaking.so.");

		/* Create new mode handler object */
		cu = new CloakUser(ServerInstance, this, MD5Module);

		/* Register it with the core */		
		ServerInstance->AddMode(cu, 'x');

		OnRehash("");
	}
	
	virtual ~ModuleCloaking()
	{
		ServerInstance->Modes->DelMode(cu);
		DELETE(cu);
	}
	
	virtual Version GetVersion()
	{
		// returns the version number of the module to be
		// listed in /MODULES
		return Version(1,1,0,2,VF_COMMON|VF_VENDOR,API_VERSION);
	}

	virtual void OnRehash(const std::string &parameter)
	{
		cu->DoRehash();
	}

	void Implements(char* List)
	{
		List[I_OnRehash] = 1;
	}
};

// stuff down here is the module-factory stuff. For basic modules you can ignore this.

class ModuleCloakingFactory : public ModuleFactory
{
 public:
	ModuleCloakingFactory()
	{
	}
	
	~ModuleCloakingFactory()
	{
	}
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleCloaking(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleCloakingFactory;
}
