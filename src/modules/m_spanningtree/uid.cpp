/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

#include "m_spanningtree/main.h"
#include "m_spanningtree/utils.h"
#include "m_spanningtree/treeserver.h"
#include "m_spanningtree/link.h"
#include "m_spanningtree/treesocket.h"
#include "m_spanningtree/resolvers.h"
#include "m_spanningtree/handshaketimer.h"

/* $ModDep: m_spanningtree/resolvers.h m_spanningtree/main.h m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/link.h m_spanningtree/treesocket.h m_hash.h m_spanningtree/handshaketimer.h */

bool TreeSocket::ParseUID(const std::string &source, std::deque<std::string> &params, const std::string &up)
{
	/** Do we have enough parameters:
	 *      0    1    2    3    4    5        6        7     8        9       (n-1)
	 * UID uuid age nick host dhost ident ip.string signon +modes (modepara) :gecos
	 */
	if (params.size() < 10)
	{
		this->SendError("Invalid client introduction (wanted 10 or more parameters, got " + (params.empty() ? "0" : ConvToStr(params.size())) + "!)");
		return false;
	}

	time_t age_t = ConvToInt(params[1]);
	time_t signon = ConvToInt(params[7]);
	std::string empty;

	TreeServer* remoteserver = Utils->FindServer(source);
	TreeServer* uplink = Utils->FindServer(up);

	if (!uplink)
	{
		this->SendError("Invalid client introduction (Can't determine name of uplink!)");
		return false;
	}
	if (!remoteserver)
	{
		this->SendError("Invalid client introduction (Unknown server "+source+")");
		return false;
	}
	/* Check parameters for validity before introducing the client, discovered by dmb */
	else if (!age_t)
	{
		this->SendError("Invalid client introduction (Invalid TS?)");
		return false;
	}
	else if (!signon)
	{
		this->SendError("Invalid client introduction (Invalid signon?)");
		return false;
	}
	else if (params[8][0] != '+')
	{
		this->SendError("Invalid client introduction (Malformed MODE sequence?)");
		return false;
	}

	/* check for collision */
	user_hash::iterator iter = this->ServerInstance->Users->clientlist->find(params[2]);

	if (iter != this->ServerInstance->Users->clientlist->end())
	{
		/*
		 * Nick collision.
		 */
		int collide = this->DoCollision(iter->second, age_t, params[5], params[8], params[0]);
		ServerInstance->Logs->Log("m_spanningtree",DEBUG,"*** Collision on %s, collide=%d", params[2].c_str(), collide);

		if (collide != 1)
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
		_new = new User(this->ServerInstance, params[0]);
	}
	catch (...)
	{
		this->SendError("Protocol violation - Duplicate UUID '" + params[0] + "' on introduction of new user");
		return false;
	}
	(*(this->ServerInstance->Users->clientlist))[params[2]] = _new;
	_new->SetFd(FD_MAGIC_NUMBER);
	_new->nick.assign(params[2], 0, MAXBUF);
	_new->host.assign(params[3], 0, 64);
	_new->dhost.assign(params[4], 0, 64);
	_new->server = this->ServerInstance->FindServerNamePtr(remoteserver->GetName().c_str());
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
		if (*v == '+')
			continue;

		/* For each mode thats set, increase counter */
		ModeHandler* mh = ServerInstance->Modes->FindMode(*v, MODETYPE_USER);

		if (mh)
		{
			if (mh->GetNumParams(true))
			{
				/* IMPORTANT NOTE:
				 * All modes are assumed to succeed here as they are being set by a remote server.
				 * Modes CANNOT FAIL here. If they DO fail, then the failure is ignored. This is important
				 * to note as all but one modules currently cannot ever fail in this situation, except for
				 * m_servprotect which specifically works this way to prevent the mode being set ANYWHERE
				 * but here, at client introduction. You may safely assume this behaviour is standard and
				 * will not change in future versions if you want to make use of this protective behaviour
				 * yourself.
				 */
				if (paramptr < params.size() - 1)
					mh->OnModeChange(_new, _new, NULL, params[paramptr++], true);
				else
				{
					this->SendError(std::string("Broken UID command, expected a parameter for user mode '")+(*v)+"' but there aren't enough parameters in the command!");
					return false;
				}
			}
			else
				mh->OnModeChange(_new, _new, NULL, empty, true);
			_new->SetMode(*v, true);
			mh->ChangeCount(1);
		}
		else
		{
			this->SendError(std::string("Warning: Broken UID command, unknown user mode '")+(*v)+"' in the mode string! (mismatched module?)");
			return false;
		}
	}

	/* now we've done with modes processing, put the + back for remote servers */
	if (params[8][0] != '+')
		params[8] = "+" + params[8];

#ifdef SUPPORT_IP6LINKS
	if (params[6].find_first_of(":") != std::string::npos)
		_new->SetSockAddr(AF_INET6, params[6].c_str(), 0);
	else
#endif
		_new->SetSockAddr(AF_INET, params[6].c_str(), 0);

	ServerInstance->Users->AddGlobalClone(_new);

	bool dosend = true;

	if ((this->Utils->quiet_bursts && (remoteserver->bursting || uplink->bursting)) || this->ServerInstance->SilentULine(_new->server))
		dosend = false;

	if (dosend)
		this->ServerInstance->SNO->WriteToSnoMask('C',"Client connecting at %s: %s!%s@%s [%s] [%s]", _new->server, _new->nick.c_str(), _new->ident.c_str(), _new->host.c_str(), _new->GetIPString(), _new->fullname.c_str());

	params[params.size() - 1] = ":" + params[params.size() - 1];
	Utils->DoOneToAllButSender(source, "UID", params, source);

	ServerInstance->PI->Introduce(_new);
	FOREACH_MOD_I(ServerInstance,I_OnPostConnect,OnPostConnect(_new));

	return true;
}

