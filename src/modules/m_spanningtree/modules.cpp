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
#include "xline.h"

#include "treesocket.h"
#include "treeserver.h"
#include "utils.h"

/* $ModDep: m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/treesocket.h */


bool TreeSocket::Modules(const std::string &prefix, parameterlist &params)
{
	if (params.empty())
		return true;

	if (!InspIRCd::Match(ServerInstance->Config->ServerName, params[0]))
	{
		/* Pass it on, not for us */
		Utils->DoOneToOne(prefix, "MODULES", params, params[0]);
		return true;
	}

	char strbuf[MAXBUF];
	parameterlist par;
	par.push_back(prefix);
	par.push_back("");

	User* source = ServerInstance->FindNick(prefix);
	if (!source)
		return true;

	std::vector<std::string> module_names = ServerInstance->Modules->GetAllModuleNames(0);

	for (unsigned int i = 0; i < module_names.size(); i++)
	{
		Module* m = ServerInstance->Modules->Find(module_names[i]);
		Version V = m->GetVersion();

		if (IS_OPER(source))
		{
			std::string flags("SvscC");
			int pos = 0;
			for (int mult = 1; mult <= VF_OPTCOMMON; mult *= 2, ++pos)
				if (!(V.Flags & mult))
					flags[pos] = '-';

			snprintf(strbuf, MAXBUF, "::%s 702 %s :0x%08lx %s %s :%s - %s", ServerInstance->Config->ServerName, source->nick.c_str(),(unsigned long)m, module_names[i].c_str(), flags.c_str(), V.description.c_str(), V.version.c_str());
		}
		else
		{
			snprintf(strbuf, MAXBUF, "::%s 702 %s :%s %s", ServerInstance->Config->ServerName, source->nick.c_str(), module_names[i].c_str(), V.description.c_str());
		}
		par[1] = strbuf;
		Utils->DoOneToOne(ServerInstance->Config->GetSID(), "PUSH", par, source->server);
	}
	snprintf(strbuf, MAXBUF, "::%s 703 %s :End of MODULES list", ServerInstance->Config->ServerName, source->nick.c_str());
	par[1] = strbuf;
	Utils->DoOneToOne(ServerInstance->Config->GetSID(), "PUSH", par, source->server);
	return true;
}

