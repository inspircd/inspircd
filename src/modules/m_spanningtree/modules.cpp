/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2008 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "xline.h"

#include "m_spanningtree/treesocket.h"
#include "m_spanningtree/treeserver.h"
#include "m_spanningtree/utils.h"

/* $ModDep: m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/treesocket.h */


bool TreeSocket::Modules(const std::string &prefix, std::deque<std::string> &params)
{
	if (params.empty())
		return true;

	if (!this->Instance->MatchText(this->Instance->Config->ServerName, params[0]))
	{
		/* Pass it on, not for us */
		Utils->DoOneToOne(prefix, "MODULES", params, params[0]);
		return true;
	}

	char strbuf[MAXBUF];
	std::deque<std::string> par;
	par.push_back(prefix);
	par.push_back("");

	User* source = this->Instance->FindNick(prefix);
	if (!source)
		return true;

	std::vector<std::string> module_names = Instance->Modules->GetAllModuleNames(0);

	for (unsigned int i = 0; i < module_names.size(); i++)
	{
		Module* m = Instance->Modules->Find(module_names[i]);
		Version V = m->GetVersion();
		char modulename[MAXBUF];
		char flagstate[MAXBUF];
		*flagstate = 0;
		if (V.Flags & VF_STATIC)
			strlcat(flagstate,", static",MAXBUF);
		if (V.Flags & VF_VENDOR)
			strlcat(flagstate,", vendor",MAXBUF);
		if (V.Flags & VF_COMMON)
			strlcat(flagstate,", common",MAXBUF);
		if (V.Flags & VF_SERVICEPROVIDER)
			strlcat(flagstate,", service provider",MAXBUF);
		if (!flagstate[0])
			strcpy(flagstate,"  <no flags>");
		strlcpy(modulename,module_names[i].c_str(),256);
		if (*source->oper)
		{
			snprintf(strbuf, MAXBUF, "::%s 900 %s :0x%08lx %d.%d.%d.%d %s (%s)",Instance->Config->ServerName,source->nick,(unsigned long)m,
					V.Major,V.Minor,V.Revision,V.Build,ServerConfig::CleanFilename(modulename),flagstate+2);
		}
		else
		{
			snprintf(strbuf, MAXBUF, "::%s 900 %s :%s",Instance->Config->ServerName,source->nick,ServerConfig::CleanFilename(modulename));
		}
		par[1] = strbuf;
		Utils->DoOneToOne(Instance->Config->GetSID(), "PUSH", par, source->server);
	}
	snprintf(strbuf, MAXBUF, "::%s 901 %s :End of MODULES list", Instance->Config->ServerName, source->nick);
	par[1] = strbuf;
	Utils->DoOneToOne(Instance->Config->GetSID(), "PUSH", par, source->server);
	return true;
}

