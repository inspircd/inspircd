/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2012 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

class ModulePassForward : public Module
{
 private:
	std::string nickrequired, forwardmsg, forwardcmd;

 public:
	ModulePassForward()
	{
		OnRehash(NULL);
		Implementation eventlist[] = { I_OnPostConnect, I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}

	Version GetVersion()
	{
		return Version("Sends server password to NickServ", VF_VENDOR);
	}

	void OnRehash(User* user)
	{
		ConfigReader Conf;
		nickrequired = Conf.ReadValue("passforward", "nick", "NickServ", 0);
		forwardmsg = Conf.ReadValue("passforward", "forwardmsg", "NOTICE $nick :*** Forwarding PASS to $nickrequired", 0);
		forwardcmd = Conf.ReadValue("passforward", "cmd", "PRIVMSG $nickrequired :IDENTIFY $pass", 0);
	}

	void FormatStr(std::string& result, const std::string& format, const std::string &nick, const std::string &pass)
	{
		for (unsigned int i = 0; i < format.length(); i++)
		{
			char c = format[i];
			if (c == '$')
			{
				if (format.substr(i, 13) == "$nickrequired")
				{
					result.append(nickrequired);
					i += 12;
				}
				else if (format.substr(i, 5) == "$nick")
				{
					result.append(nick);
					i += 4;
				}
				else if (format.substr(i,5) == "$pass")
				{
					result.append(pass);
					i += 4;
				}
				else
					result.push_back(c);
			}
			else
				result.push_back(c);
		}
	}

	virtual void OnPostConnect(User* ruser)
	{
		LocalUser* user = IS_LOCAL(ruser);
		if (!user || user->password.empty())
			return;

		if (!nickrequired.empty())
		{
			/* Check if nick exists and its server is ulined */
			User* u = ServerInstance->FindNick(nickrequired.c_str());
			if (!u || !ServerInstance->ULine(u->server))
				return;
		}

		std::string tmp;
		FormatStr(tmp,forwardmsg, user->nick, user->password);
		user->WriteServ(tmp);

		tmp.clear();
		FormatStr(tmp,forwardcmd, user->nick, user->password);
		ServerInstance->Parser->ProcessBuffer(tmp,user);
	}
};

MODULE_INIT(ModulePassForward)

