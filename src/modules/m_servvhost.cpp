/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

/* $ModDesc: ircd vhosting. */

class ModuleServVHost : public Module
{
 public:
	StringExtItem cli_name, srv_name;
	ModuleServVHost() : cli_name("usercmd_host", this), srv_name("usercmd_server", this) {}

	void init()
	{
		Implementation eventlist[] = { I_OnPreCommand, I_OnSetConnectClass };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}

	Version GetVersion()
	{
		return Version("IRCd virtual host matching for connect blocks",VF_VENDOR);
	}

	ModResult OnPreCommand(std::string &command, std::vector<std::string> &parameters, LocalUser *user, bool validated, const std::string&)
	{
		if (command != "USER" || !validated)
			return MOD_RES_PASSTHRU;
		srv_name.set(user, parameters[1]);
		cli_name.set(user, parameters[2]);
		return MOD_RES_PASSTHRU;
	}

	ModResult OnSetConnectClass(LocalUser* user, ConnectClass* myclass)
	{
		std::string sname = myclass->config->getString("servername");
		std::string* myname = srv_name.get(user);
		if (!sname.empty() && myname && !InspIRCd::Match(*myname, sname))
			return MOD_RES_DENY;
		return MOD_RES_PASSTHRU;
	}
};

MODULE_INIT(ModuleServVHost)
