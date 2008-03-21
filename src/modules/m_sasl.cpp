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

/* $ModDesc: Provides support for atheme SASL via AUTHENTICATE. */

class CommandAuthenticate : public Command
{
 public:
	CommandAuthenticate (InspIRCd* Instance) : Command(Instance,"AUTHENTICATE", 0, 1)
	{
		this->source = "m_sasl.so";
	}

	CmdResult Handle (const char* const* parameters, int pcnt, User *user)
	{
		if (user->registered != REG_ALL)
		{
			/* Only allow AUTHENTICATE on unregistered clients */
			std::deque<std::string> params;
			params.push_back("*");
			for (int i = 0; i < pcnt; ++i)
				params.push_back(parameters[0]);
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
		Implementation eventlist[] = { I_OnEvent };
		ServerInstance->Modules->Attach(eventlist, this, 1);

		sasl = new CommandAuthenticate(ServerInstance);
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
			/* Received encap reply, look for AUTHENTICATE */
			std::deque<std::string>* parameters = (std::deque<std::string>*)ev->GetData();

			User* target = ServerInstance->FindNick((*parameters)[0]);

			if (target)
			{
				/* Found a user */
				parameters->pop_front();
				std::string line = irc::stringjoiner(" ", *parameters, 0, parameters->size() - 1).GetJoined();
				target->WriteServ("AUTHENTICATE %s", line.c_str());
			}
		}
		else if (ev->GetEventID() == "account_login")
		{
			AccountData* ac = (AccountData*)ev->GetData();
			ac->user->WriteServ("903 %s :SASL authentication successful", ac->user->nick);
		}
	}
};

MODULE_INIT(ModuleSASL)
