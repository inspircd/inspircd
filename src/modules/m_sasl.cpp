/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "m_cap.h"
#include "account.h"

/* $ModDesc: Provides support for IRC Authentication Layer (aka: atheme SASL) via AUTHENTICATE. */

enum SaslState { SASL_INIT, SASL_COMM, SASL_DONE };
enum SaslResult { SASL_OK, SASL_FAIL, SASL_ABORT };

/**
 * Tracks SASL authentication state like charybdis does. --nenolod
 */
class SaslAuthenticator
{
 private:
	InspIRCd *ServerInstance;
	std::string agent;
	User *user;
	SaslState state;
	SaslResult result;
	bool state_announced;

 public:
	SaslAuthenticator(User *user_, std::string method, Module *ctor)
		: user(user_), state(SASL_INIT), state_announced(false)
	{
		parameterlist params;
		params.push_back("*");
		params.push_back("SASL");
		params.push_back(user->uuid);
		params.push_back("*");
		params.push_back("S");
		params.push_back(method);

		ServerInstance->PI->SendEncapsulatedData(params);
	}

	SaslResult GetSaslResult(const std::string &result_)
	{
		if (result_ == "F")
			return SASL_FAIL;

		if (result_ == "A")
			return SASL_ABORT;

		return SASL_OK;
	}

	/* checks for and deals with a state change. */
	SaslState ProcessInboundMessage(const std::vector<std::string> &msg)
	{
		switch (this->state)
		{
		 case SASL_INIT:
			this->agent = msg[0];
			this->user->Write("AUTHENTICATE %s", msg[3].c_str());
			this->state = SASL_COMM;
			break;
		 case SASL_COMM:
			if (msg[0] != this->agent)
				return this->state;

			if (msg[2] != "D")
				this->user->Write("AUTHENTICATE %s", msg[3].c_str());
			else
			{
				this->state = SASL_DONE;
				this->result = this->GetSaslResult(msg[3]);
			}

			break;
		 case SASL_DONE:
			break;
		 default:
			ServerInstance->Logs->Log("m_sasl", DEFAULT, "WTF: SaslState is not a known state (%d)", this->state);
			break;
		}

		return this->state;
	}

	void Abort(void)
	{
		this->state = SASL_DONE;
		this->result = SASL_ABORT;
	}

	bool SendClientMessage(const std::vector<std::string>& parameters)
	{
		if (this->state != SASL_COMM)
			return true;

		parameterlist params;
		params.push_back("*");
		params.push_back("SASL");
		params.push_back(this->user->uuid);
		params.push_back(this->agent);
		params.push_back("C");

		params.insert(params.end(), parameters.begin(), parameters.end());

		ServerInstance->PI->SendEncapsulatedData(params);

		if (parameters[0][0] == '*')
		{
			this->Abort();
			return false;
		}

		return true;
	}

	void AnnounceState(void)
	{
		if (this->state_announced)
			return;

		switch (this->result)
		{
		 case SASL_OK:
			this->user->WriteNumeric(903, "%s :SASL authentication successful", this->user->nick.c_str());
			break;
	 	 case SASL_ABORT:
			this->user->WriteNumeric(906, "%s :SASL authentication aborted", this->user->nick.c_str());
			break;
		 case SASL_FAIL:
			this->user->WriteNumeric(904, "%s :SASL authentication failed", this->user->nick.c_str());
			break;
		 default:
			break;
		}

		this->state_announced = true;
	}
};

class CommandAuthenticate : public Command
{
 public:
	SimpleExtItem<SaslAuthenticator>& authExt;
	GenericCap& cap;
	CommandAuthenticate(Module* Creator, SimpleExtItem<SaslAuthenticator>& ext, GenericCap& Cap)
		: Command(Creator, "AUTHENTICATE", 1), authExt(ext), cap(Cap)
	{
		works_before_reg = true;
	}

	CmdResult Handle (const std::vector<std::string>& parameters, User *user)
	{
		/* Only allow AUTHENTICATE on unregistered clients */
		if (user->registered != REG_ALL)
		{
			if (!cap.ext.get(user))
				return CMD_FAILURE;

			SaslAuthenticator *sasl = authExt.get(user);
			if (!sasl)
				authExt.set(user, new SaslAuthenticator(user, parameters[0], creator));
			else if (sasl->SendClientMessage(parameters) == false)	// IAL abort extension --nenolod
			{
				sasl->AnnounceState();
				authExt.unset(user);
			}
		}
		return CMD_FAILURE;
	}
};

class CommandSASL : public Command
{
 public:
	SimpleExtItem<SaslAuthenticator>& authExt;
	CommandSASL(Module* Creator, SimpleExtItem<SaslAuthenticator>& ext) : Command(Creator, "SASL", 2), authExt(ext)
	{
		this->disabled = true; // should not be called by users
	}

	CmdResult Handle(const std::vector<std::string>& parameters, User *user)
	{
		User* target = ServerInstance->FindNick(parameters[1]);
		if (!target)
		{
			ServerInstance->Logs->Log("m_sasl", DEBUG,"User not found in sasl ENCAP event: %s", parameters[1].c_str());
			return CMD_FAILURE;
		}

		SaslAuthenticator *sasl = authExt.get(target);
		if (!sasl)
			return CMD_FAILURE;

		SaslState state = sasl->ProcessInboundMessage(parameters);
		if (state == SASL_DONE)
		{
			sasl->AnnounceState();
			authExt.unset(target);
		}
		return CMD_SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		return ROUTE_BROADCAST;
	}
};

class ModuleSASL : public Module
{
	SimpleExtItem<SaslAuthenticator> authExt;
	GenericCap cap;
	CommandAuthenticate auth;
	CommandSASL sasl;
 public:
	ModuleSASL()
		: authExt("sasl_auth", this), cap(this, "sasl"), auth(this, authExt, cap), sasl(this, authExt)
	{
		Implementation eventlist[] = { I_OnEvent, I_OnUserRegister };
		ServerInstance->Modules->Attach(eventlist, this, 2);

		ServiceProvider* providelist[] = { &auth, &sasl, &authExt };
		ServerInstance->Modules->AddServices(providelist, 3);

		if (!ServerInstance->Modules->Find("m_services_account.so") || !ServerInstance->Modules->Find("m_cap.so"))
			ServerInstance->Logs->Log("m_sasl", DEFAULT, "WARNING: m_services_account.so and m_cap.so are not loaded! m_sasl.so will NOT function correctly until these two modules are loaded!");
	}

	ModResult OnUserRegister(LocalUser *user)
	{
		SaslAuthenticator *sasl_ = authExt.get(user);
		if (sasl_)
		{
			sasl_->Abort();
			authExt.unset(user);
		}

		return MOD_RES_PASSTHRU;
	}

	~ModuleSASL()
	{
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
