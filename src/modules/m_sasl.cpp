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
	Module* Creator;
 public:
	CommandAuthenticate (InspIRCd* Instance, Module* creator) : Command(Instance,"AUTHENTICATE", 0, 1, true), Creator(creator)
	{
		this->source = "m_sasl.so";
	}

	CmdResult Handle (const char* const* parameters, int pcnt, User *user)
	{
		if (user->registered != REG_ALL)
		{
			/* Only act if theyve enabled CAP REQ sasl */
			if (user->GetExt("sasl"))
			{
				/* Only allow AUTHENTICATE on unregistered clients */
				std::deque<std::string> params;
				params.push_back("*");
				params.push_back("AUTHENTICATE");
				params.push_back(user->uuid);

				for (int i = 0; i < pcnt; ++i)
					params.push_back(parameters[i]);

				Event e((char*)&params, Creator, "send_encap");
				e.Send(ServerInstance);
			}
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

		sasl = new CommandAuthenticate(ServerInstance, this);
		ServerInstance->AddCommand(sasl);

		if (!ServerInstance->Modules->Find("m_services_account.so") || !ServerInstance->Modules->Find("m_cap.so"))
			ServerInstance->Logs->Log("m_sasl", DEFAULT, "WARNING: m_services_account.so and m_cap.so are not loaded! m_sasl.so will NOT function correctly until these two modules are loaded!");
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

			if (ac->user->GetExt("sasl"))
			{
				ac->user->WriteServ("903 %s :SASL authentication successful", ac->user->nick);
				ac->user->Shrink("sasl");
			}
		}
	}
};

MODULE_INIT(ModuleSASL)
