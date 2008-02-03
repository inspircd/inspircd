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
#include "commands/cmd_whois.h"
#include "commands/cmd_stats.h"
#include "socket.h"
#include "wildcard.h"
#include "xline.h"
#include "transport.h"
#include "m_hash.h"
#include "socketengine.h"

#include "m_spanningtree/main.h"
#include "m_spanningtree/utils.h"
#include "m_spanningtree/treeserver.h"
#include "m_spanningtree/link.h"
#include "m_spanningtree/treesocket.h"
#include "m_spanningtree/resolvers.h"
#include "m_spanningtree/handshaketimer.h"

/* $ModDep: m_spanningtree/timesynctimer.h m_spanningtree/resolvers.h m_spanningtree/main.h m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/link.h m_spanningtree/treesocket.h m_hash.h */

bool TreeSocket::ParseUID(const std::string &source, std::deque<std::string> &params)
{
	/** Do we have enough parameters:
	 * UID uuid age nick host dhost ident +modestr ip.string :gecos
	 */
	if (params.size() != 10)
	{
		this->WriteLine(std::string(":")+this->Instance->Config->GetSID()+" KILL "+params[0]+" :Invalid client introduction ("+params[0]+" with only "+
				ConvToStr(params.size())+" of 10 parameters?)");
		return true;
	}

	time_t age = ConvToInt(params[1]);
	time_t signon = ConvToInt(params[8]);
	const char* tempnick = params[2].c_str();
	std::string empty;

	/* XXX probably validate UID length too -- w00t */
	cmd_validation valid[] = { {"Nickname", 2, NICKMAX}, {"Hostname", 3, 64}, {"Displayed hostname", 4, 64}, {"Ident", 5, IDENTMAX}, {"GECOS", 9, MAXGECOS}, {"", 0, 0} };

	TreeServer* remoteserver = Utils->FindServer(source);

	if (!remoteserver)
	{
		this->WriteLine(std::string(":")+this->Instance->Config->GetSID()+" KILL "+params[0]+" :Invalid client introduction (Unknown server "+source+")");
		return true;
	}

	/* Check parameters for validity before introducing the client, discovered by dmb */
	if (!age)
	{
		this->WriteLine(std::string(":")+this->Instance->Config->GetSID()+" KILL "+params[0]+" :Invalid client introduction (Invalid TS?)");
		return true;
	}

	for (size_t x = 0; valid[x].length; ++x)
	{
		if (params[valid[x].param].length() > valid[x].length)
		{
			this->WriteLine(std::string(":")+this->Instance->Config->GetSID()+" KILL "+params[0]+" :Invalid client introduction (" + valid[x].item + " > " + ConvToStr(valid[x].length) + ")");
			return true;
		}
	}


	/* check for collision */
	user_hash::iterator iter = this->Instance->Users->clientlist->find(tempnick);

	if (iter != this->Instance->Users->clientlist->end())
	{
		/*
		 * Nick collision.
		 */
		Instance->Log(DEBUG,"*** Collision on %s", tempnick);
		int collide = this->DoCollision(iter->second, age, params[5].c_str(), params[7].c_str(), params[0].c_str());

		if (collide == 2)
		{
			/* remote client changed, make sure we change their nick for the hash too */
			tempnick = params[0].c_str();
		}
	}

	/* IMPORTANT NOTE: For remote users, we pass the UUID in the constructor. This automatically
	 * sets it up in the UUID hash for us.
	 */
	User* _new = NULL;
	try
	{
		_new = new User(this->Instance, params[0]);
	}
	catch (...)
	{
		SendError("Protocol violation - Duplicate UUID '" + params[0] + "' on introduction of new user");
		return false;
	}
	(*(this->Instance->Users->clientlist))[tempnick] = _new;
	_new->SetFd(FD_MAGIC_NUMBER);
	strlcpy(_new->nick, tempnick, NICKMAX - 1);
	strlcpy(_new->host, params[3].c_str(),64);
	strlcpy(_new->dhost, params[4].c_str(),64);
	_new->server = this->Instance->FindServerNamePtr(remoteserver->GetName().c_str());
	strlcpy(_new->ident, params[5].c_str(),IDENTMAX);
	strlcpy(_new->fullname, params[9].c_str(),MAXGECOS);
	_new->registered = REG_ALL;
	_new->signon = signon;
	_new->age = age;

	/* we need to remove the + from the modestring, so we can do our stuff */
	std::string::size_type pos_after_plus = params[6].find_first_not_of('+');
	if (pos_after_plus != std::string::npos)
	params[6] = params[6].substr(pos_after_plus);

	for (std::string::iterator v = params[6].begin(); v != params[6].end(); v++)
	{
		/* For each mode thats set, increase counter */
		ModeHandler* mh = Instance->Modes->FindMode(*v, MODETYPE_USER);

		if (mh)
		{
			mh->OnModeChange(_new, _new, NULL, empty, true);
			_new->SetMode(*v, true);
			mh->ChangeCount(1);
		}
	}

	/* now we've done with modes processing, put the + back for remote servers */
	params[6] = "+" + params[6];

#ifdef SUPPORT_IP6LINKS
	if (params[7].find_first_of(":") != std::string::npos)
		_new->SetSockAddr(AF_INET6, params[7].c_str(), 0);
	else
#endif
		_new->SetSockAddr(AF_INET, params[7].c_str(), 0);

	Instance->Users->AddGlobalClone(_new);

	bool dosend = true;
	
	if ((this->Utils->quiet_bursts && remoteserver->bursting) || this->Instance->SilentULine(_new->server))
		dosend = false;
	
	if (dosend)
		this->Instance->SNO->WriteToSnoMask('C',"Client connecting at %s: %s!%s@%s [%s] [%s]",_new->server,_new->nick,_new->ident,_new->host, _new->GetIPString(), _new->fullname);

	params[9] = ":" + params[9];
	Utils->DoOneToAllButSender(source, "UID", params, source);

	FOREACH_MOD_I(Instance,I_OnPostConnect,OnPostConnect(_new));

	return true;
}

