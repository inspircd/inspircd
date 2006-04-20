/*   +------------------------------------+
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

#include "inspircd.h"
#include "configreader.h"
#include "users.h"
#include "commands.h"
#include "helperfuncs.h"
#include "commands/cmd_version.h"

extern ServerConfig* Config;
extern InspIRCd* ServerInstance;

void cmd_version::Handle (char **parameters, int pcnt, userrec *user)
{
	std::stringstream out(Config->data005);
	std::string token = "";
	std::string line5 = "";
	int token_counter = 0;

	WriteServ(user->fd,"351 %s :%s",user->nick,ServerInstance->GetVersionString().c_str());

	while (!out.eof())
	{
		out >> token;
		line5 = line5 + token + " ";
		token_counter++;

		if ((token_counter >= 13) || (out.eof() == true))
		{
			WriteServ(user->fd,"005 %s %s:are supported by this server",user->nick,line5.c_str());
			line5 = "";
			token_counter = 0;
		}
	}
}
