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

/* $ModDep: m_spanningtree/resolvers.h m_spanningtree/main.h m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/link.h m_spanningtree/treesocket.h m_hash.h m_spanningtree/handshaketimer.h */

bool TreeSocket::ParseUID(const std::string &source, std::deque<std::string> &params)
{
	/** Do we have enough parameters:
	 *      0    1    2    3    4    5        6        7     8        9       (n-1)
	 * UID uuid age nick host dhost ident ip.string signon +modes (modepara) :gecos
	 */
	if (params.size() < 10)
	{
		if (!params.empty())
			this->WriteLine(std::string(":")+this->Instance->Config->GetSID()+" KILL "+params[0]+" :Invalid client introduction ("+params[0]+" with only "+
					ConvToStr(params.size())+" of 10 or more parameters?)");
		return true;
	}

	time_t age_t = ConvToInt(params[1]);
	time_t signon = ConvToInt(params[7]);
	std::string empty;

	TreeServer* remoteserver = Utils->FindServer(source);

	if (!remoteserver)
	{
		this->WriteLine(std::string(":")+this->Instance->Config->GetSID()+" KILL "+params[0]+" :Invalid client introduction (Unknown server "+source+")");
		return true;
	}

	/* Check parameters for validity before introducing the client, discovered by dmb */
	if (!age_t)
	{
		this->WriteLine(std::string(":")+this->Instance->Config->GetSID()+" KILL "+params[0]+" :Invalid client introduction (Invalid TS?)");
		return true;
	}

	/* check for collision */
	user_hash::iterator iter = this->Instance->Users->clientlist->find(params[2]);

	if (iter != this->Instance->Users->clientlist->end())
	{
		/*
		 * Nick collision.
		 */
		Instance->Logs->Log("m_spanningtree",DEBUG,"*** Collision on %s", params[2].c_str());
		int collide = this->DoCollision(iter->second, age_t, params[5], params[8], params[0]);

		if (collide == 2)
		{
			/* remote client changed, make sure we change their nick for the hash too */
			params[2] = params[0];
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
	(*(this->Instance->Users->clientlist))[params[2]] = _new;
	_new->SetFd(FD_MAGIC_NUMBER);
	_new->nick.assign(params[2], 0, MAXBUF);
	_new->host.assign(params[3], 0, 64);
	_new->dhost.assign(params[4], 0, 64);
	_new->server = this->Instance->FindServerNamePtr(remoteserver->GetName().c_str());
	_new->ident.assign(params[5], 0, MAXBUF);
	_new->fullname.assign(params[params.size() - 1], 0, MAXBUF);
	_new->registered = REG_ALL;
	_new->signon = signon;
	_new->age = age_t;

	/* we need to remove the + from the modestring, so we can do our stuff */
	std::string::size_type pos_after_plus = params[8].find_first_not_of('+');
	if (pos_after_plus != std::string::npos)
	params[8] = params[8].substr(pos_after_plus);

	unsigned int paramptr = 9;
	for (std::string::iterator v = params[8].begin(); v != params[8].end(); v++)
	{
		/* For each mode thats set, increase counter */
		ModeHandler* mh = Instance->Modes->FindMode(*v, MODETYPE_USER);

		if (mh)
		{
			if (mh->GetNumParams(true) && (paramptr < params.size() - 1))
				mh->OnModeChange(_new, _new, NULL, params[paramptr++], true);
			else
				mh->OnModeChange(_new, _new, NULL, empty, true);
			_new->SetMode(*v, true);
			mh->ChangeCount(1);
		}
	}

	//_new->ProcessNoticeMasks(params[7].c_str());

	/* now we've done with modes processing, put the + back for remote servers */
	params[8] = "+" + params[8];

#ifdef SUPPORT_IP6LINKS
	if (params[6].find_first_of(":") != std::string::npos)
		_new->SetSockAddr(AF_INET6, params[6].c_str(), 0);
	else
#endif
		_new->SetSockAddr(AF_INET, params[6].c_str(), 0);

	Instance->Users->AddGlobalClone(_new);

	bool dosend = true;

	if ((this->Utils->quiet_bursts && remoteserver->bursting) || this->Instance->SilentULine(_new->server))
		dosend = false;

	if (dosend)
		this->Instance->SNO->WriteToSnoMask('C',"Client connecting at %s: %s!%s@%s [%s] [%s]", _new->server, _new->nick.c_str(), _new->ident.c_str(), _new->host.c_str(), _new->GetIPString(), _new->fullname.c_str());

	params[params.size() - 1] = ":" + params[params.size() - 1];
	Utils->DoOneToAllButSender(source, "UID", params, source);

	Instance->PI->Introduce(_new);
	FOREACH_MOD_I(Instance,I_OnPostConnect,OnPostConnect(_new));

	return true;
}

