/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2008 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
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
	Module *Creator;
	std::string agent;
	User *user;
	SaslState state;
	SaslResult result;
	bool state_announced;

 public:
	SaslAuthenticator(User *user, std::string method, InspIRCd *instance, Module *ctor)
		: ServerInstance(instance), Creator(ctor), user(user), state(SASL_INIT)
	{
		this->user->Extend("sasl_authenticator", this);

		std::deque<std::string> params;
		params.push_back("*");
		params.push_back("SASL");
		params.push_back(user->uuid);
		params.push_back("*");
		params.push_back("S");
		params.push_back(method);

		Event e((char*)&params, Creator, "send_encap");
		e.Send(ServerInstance);
	}

	SaslResult GetSaslResult(std::string &result)
	{
		if (result == "F")
			return SASL_FAIL;

		if (result == "A")
			return SASL_ABORT;

		return SASL_OK;
	}

	/* checks for and deals with a state change. */
	SaslState ProcessInboundMessage(std::deque<std::string> &msg)
	{
		switch (this->state)
		{
		 case SASL_INIT:
			this->agent = msg[1];
			this->user->Write("AUTHENTICATE %s", msg[5].c_str());
			this->state = SASL_COMM;
			break;
		 case SASL_COMM:
			if (msg[1] != this->agent)
				return this->state;

			if (msg[4] != "D")
				this->user->Write("AUTHENTICATE %s", msg[5].c_str());
			else
			{
				this->state = SASL_DONE;
				this->result = this->GetSaslResult(msg[5]);
				this->AnnounceState();
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

	bool SendClientMessage(const char* const* parameters, int pcnt)
	{
		if (this->state != SASL_COMM)
			return;

		std::deque<std::string> params;
		params.push_back("*");
		params.push_back("SASL");
		params.push_back(this->user->uuid);
		params.push_back(this->agent);
		params.push_back("C");

		for (int i = 0; i < pcnt; ++i)
			params.push_back(parameters[i]);		

		Event e((char*)&params, Creator, "send_encap");
		e.Send(ServerInstance);

		if (*parameters[0] == '*')
		{
			this->state = SASL_DONE;
			this->result = SASL_ABORT;

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
			this->user->WriteServ("903 %s :SASL authentication successful", this->user->nick);
			break;
	 	 case SASL_ABORT:
			this->user->WriteServ("906 %s :SASL authentication aborted", this->user->nick);
			break;
		 case SASL_FAIL:
			this->user->WriteServ("904 %s :SASL authentication failed", this->user->nick);
			break;
		 default:
			break;
		}
	}

	~SaslAuthenticator()
	{
		this->user->Shrink("sasl_authenticator");
		this->AnnounceState();
	}
};

class CommandAuthenticate : public Command
{
	Module* Creator;
 public:
	CommandAuthenticate (InspIRCd* Instance, Module* creator) : Command(Instance,"AUTHENTICATE", 0, 1, true), Creator(creator)
	{
		this->source = "m_sasl.so";
	}

	CmdResult Handle (const char* const* parameters, int pcnt, User *user)
	{
		/* Only allow AUTHENTICATE on unregistered clients */
		if (user->registered != REG_ALL)
		{
			if (!user->GetExt("sasl"))
				return CMD_FAILURE;

			SaslAuthenticator *sasl;
			if (!(user->GetExt("sasl_authenticator", sasl)))
				sasl = new SaslAuthenticator(user, parameters[0], ServerInstance, Creator);
			else if (sasl->SendClientMessage(parameters, pcnt) == false)	// IAL abort extension --nenolod
				delete sasl;
		}
		return CMD_FAILURE;
	}
};


class ModuleSASL : public Module
{
	CommandAuthenticate* sasl;
 public:
	
	ModuleSASL(InspIRCd* Me)
		: Module(Me)
	{
		Implementation eventlist[] = { I_OnEvent, I_OnUserRegister, I_OnPostConnect };
		ServerInstance->Modules->Attach(eventlist, this, 3);

		sasl = new CommandAuthenticate(ServerInstance, this);
		ServerInstance->AddCommand(sasl);

		if (!ServerInstance->Modules->Find("m_services_account.so") || !ServerInstance->Modules->Find("m_cap.so"))
			ServerInstance->Logs->Log("m_sasl", DEFAULT, "WARNING: m_services_account.so and m_cap.so are not loaded! m_sasl.so will NOT function correctly until these two modules are loaded!");
	}

	virtual int OnUserRegister(User *user)
	{
		if (user->GetExt("sasl"))
		{
			user->WriteServ("906 %s :SASL authentication aborted", user->nick);
			user->Shrink("sasl");
		}

		return 0;
	}

	virtual void OnPostConnect(User* user)
	{
		if (!IS_LOCAL(user))
			return;

		std::string* str = NULL;

		if (user->GetExt("accountname", str))
		{
			std::deque<std::string> params;
			params.push_back(user->uuid);
			params.push_back("accountname");
			params.push_back(*str);
			Event e((char*)&params, this, "send_metadata");
			e.Send(ServerInstance);
		}
		return;
	}

	virtual ~ModuleSASL()
	{
	}

	virtual Version GetVersion()
	{
		return Version(1,2,0,1,VF_VENDOR,API_VERSION);
	}

	virtual void OnEvent(Event *ev)
	{
		GenericCapHandler(ev, "sasl", "sasl");

		if (ev->GetEventID() == "encap_received")
		{
			std::deque<std::string>* parameters = (std::deque<std::string>*)ev->GetData();

			if ((*parameters)[1] != "SASL")
				return;

			User* target = ServerInstance->FindNick((*parameters)[2]);
			if (!target)
			{
				ServerInstance->Logs->Log("m_sasl", DEBUG,"User not found in sasl ENCAP event: %s", (*parameters)[2].c_str());
				return;
			}

			SaslAuthenticator *sasl;
			if (!target->GetExt("sasl_authenticator", sasl))
				return;

			SaslState state = sasl->ProcessInboundMessage(*parameters);
			if (state == SASL_DONE)
				delete sasl;
		}
	}
};

MODULE_INIT(ModuleSASL)
