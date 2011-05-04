/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2011 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "protocol.h"
#include "m_cap.h"
#include "account.h"
#include "sasl.h"

/* $ModDesc: Provides support for IRC Authentication Layer (aka: atheme SASL) via AUTHENTICATE. */

class SaslAuthenticator;
static std::string sasl_target = "*";
static SimpleExtItem<SASLHook>* sasl_ext;

/** Pass-thru to server */
class SaslAuthenticator : public SASLHook
{
	std::string agent;

 public:
	SaslAuthenticator(User* user, const std::string& method)
	{
		parameterlist params;
		params.push_back(sasl_target);
		params.push_back("SASL");
		params.push_back(user->uuid);
		params.push_back("*");
		params.push_back("S");
		params.push_back(method);

		ServerInstance->PI->SendEncapsulatedData(params);
	}

	void ProcessClient(User* user, const std::vector<std::string>& parameters)
	{
		parameterlist params;
		params.push_back(sasl_target);
		params.push_back("SASL");
		params.push_back(user->uuid);
		params.push_back(agent);
		params.push_back("C");

		params.insert(params.end(), parameters.begin(), parameters.end());

		ServerInstance->PI->SendEncapsulatedData(params);

		if (parameters[0][0] == '*')
		{
			user->WriteNumeric(906, "%s :SASL authentication aborted", user->nick.c_str());
			sasl_ext->unset(user);
		}
	}

	void ProcessServer(User* user, const std::vector<std::string> &msg)
	{
		if (agent.empty())
			agent = msg[0];
		else if (agent != msg[0])
			return;

		if (msg[2] == "D")
		{
			// SASL DONE requires us to translate to a numeric
			if (msg[3] == "F")
			{
				user->WriteNumeric(904, "%s :SASL authentication failed", user->nick.c_str());
				sasl_ext->unset(user);
			}
			else if (msg[3] == "A")
			{
				user->WriteNumeric(906, "%s :SASL authentication aborted", user->nick.c_str());
				sasl_ext->unset(user);
			}
			else
			{
				user->WriteNumeric(903, "%s :SASL authentication successful", user->nick.c_str());
				sasl_ext->unset(user);
			}
		}
		else
		{
			user->Write("AUTHENTICATE %s", msg[3].c_str());
		}
	}
};

class CommandAuthenticate : public Command
{
 public:
	CommandAuthenticate(Module* Creator) : Command(Creator, "AUTHENTICATE", 1)
	{
		works_before_reg = true;
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		/* Only allow AUTHENTICATE on unregistered clients */
		if (user->registered == REG_ALL)
			return CMD_FAILURE;
		SASLHook *sasl = sasl_ext->get(user);
		if (!sasl)
		{
			SASLSearch search(creator, user, parameters[0]);
			if (search.auth)
				sasl = search.auth;
			else
				sasl = new SaslAuthenticator(user, parameters[0]);
			sasl_ext->set(user, sasl);
		}
		else
		{
			sasl->ProcessClient(user, parameters);
		}
		return CMD_SUCCESS;
	}
};

class CommandSASL : public Command
{
 public:
	CommandSASL(Module* Creator) : Command(Creator, "SASL", 2)
	{
		flags_needed = FLAG_SERVERONLY; // should not be called by users
	}

	CmdResult Handle(const std::vector<std::string>& parameters, User *user)
	{
		User* target = ServerInstance->FindNick(parameters[1]);
		if (!target)
		{
			ServerInstance->Logs->Log("m_sasl", DEBUG,"User not found in sasl ENCAP event: %s", parameters[1].c_str());
			return CMD_FAILURE;
		}

		SASLHook *sasl = sasl_ext->get(target);
		if (!sasl)
		{
			ServerInstance->Logs->Log("m_sasl", DEBUG, "SASL ext not found in sasl ENCAP event for %s", parameters[1].c_str());
			return CMD_FAILURE;
		}

		sasl->ProcessServer(user, parameters);
		return CMD_SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		return ROUTE_BROADCAST;
	}
};

class ModuleSASL : public Module
{
	SimpleExtItem<SASLHook> authExt;
	GenericCap cap;
	CommandAuthenticate auth;
	CommandSASL sasl;
 public:
	ModuleSASL()
		: authExt(EXTENSIBLE_USER, "sasl_auth", this), cap(this, "sasl"), auth(this), sasl(this)
	{
		sasl_ext = &authExt;
	}

	void init()
	{
		Implementation eventlist[] = { I_OnEvent, I_OnUserRegister };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));

		ServiceProvider* providelist[] = { &auth, &sasl, &authExt };
		ServerInstance->Modules->AddServices(providelist, 3);
	}

	void ReadConfig(ConfigReadStatus&)
	{
		sasl_target = ServerInstance->Config->GetTag("sasl")->getString("target", "*");
	}

	void OnUserRegister(LocalUser *user)
	{
		SASLHook *sasl_ = authExt.get(user);
		if (sasl_)
		{
			user->WriteNumeric(906, "%s :SASL authentication aborted", user->nick.c_str());
			authExt.unset(user);
		}
	}

	Version GetVersion()
	{
		return Version("Provides support for IRC Authentication Layer (aka: atheme SASL) via AUTHENTICATE.",VF_VENDOR);
	}

	void OnEvent(Event &ev)
	{
		cap.HandleEvent(ev);
	}
};

MODULE_INIT(ModuleSASL)
