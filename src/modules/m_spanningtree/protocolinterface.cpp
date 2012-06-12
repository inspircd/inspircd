/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Craig Edwards <craigedwards@brainbox.cc>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "inspircd.h"
#include "m_spanningtree/main.h"
#include "m_spanningtree/utils.h"
#include "m_spanningtree/treeserver.h"
#include "m_spanningtree/treesocket.h"
#include "m_spanningtree/protocolinterface.h"

/*
 * For documentation on this class, see include/protocol.h.
 */

void SpanningTreeProtocolInterface::GetServerList(ProtoServerList &sl)
{
	sl.clear();
	for (server_hash::iterator i = Utils->serverlist.begin(); i != Utils->serverlist.end(); i++)
	{
		ProtoServer ps;
		ps.servername = i->second->GetName();
		TreeServer* s = i->second->GetParent();
		ps.parentname = s ? s->GetName() : "";
		ps.usercount = i->second->GetUserCount();
		ps.opercount = i->second->GetOperCount();
		ps.gecos = i->second->GetDesc();
		ps.latencyms = i->second->rtt;
		sl.push_back(ps);
	}
}

void SpanningTreeProtocolInterface::SendEncapsulatedData(parameterlist &encap)
{
	if (encap.size() < 2)
		return;

	const std::string& target = encap[0];
	if (target.find_first_of("*?") == std::string::npos)
		Utils->DoOneToOne(ServerInstance->Config->GetSID(), "ENCAP", encap, target);
	else
		Utils->DoOneToMany(ServerInstance->Config->GetSID(), "ENCAP", encap);
}

void SpanningTreeProtocolInterface::SendMetaData(void* target, TargetTypeFlags type, const std::string &key, const std::string &data)
{
	parameterlist params;

	switch (type)
	{
		case TYPE_USER:
			params.push_back(((User*)target)->uuid);
			break;
		case TYPE_CHANNEL:
			params.push_back(((Channel*)target)->name);
			break;
		case TYPE_OTHER:
		case TYPE_SERVER:
			params.push_back("*");
			break;
	}
	params.push_back(key);
	params.push_back(":" + data);

	Utils->DoOneToMany(ServerInstance->Config->GetSID(),"METADATA",params);
}

void SpanningTreeProtocolInterface::SendTopic(Channel* channel, std::string &topic)
{
	parameterlist params;

	params.push_back(channel->name);
	params.push_back(ConvToStr(ServerInstance->Time()));
	params.push_back(ServerInstance->Config->ServerName);
	params.push_back(":" + topic);

	Utils->DoOneToMany(ServerInstance->Config->GetSID(),"FTOPIC", params);
}

void SpanningTreeProtocolInterface::SendMode(const std::string &target, const parameterlist &modedata, const std::deque<TranslateType> &translate)
{
	if (modedata.empty())
		return;

	std::string outdata;
	ServerInstance->Parser->TranslateUIDs(translate, modedata, outdata);

	std::string uidtarget;
	ServerInstance->Parser->TranslateUIDs(TR_NICK, target, uidtarget);

	parameterlist outlist;
	outlist.push_back(uidtarget);
	outlist.push_back(outdata);

	User* a = ServerInstance->FindNick(uidtarget);
	if (a)
	{
		Utils->DoOneToMany(ServerInstance->Config->GetSID(),"MODE",outlist);
		return;
	}
	else
	{
		Channel* c = ServerInstance->FindChan(target);
		if (c)
		{
			outlist.insert(outlist.begin() + 1, ConvToStr(c->age));
			Utils->DoOneToMany(ServerInstance->Config->GetSID(),"FMODE",outlist);
		}
	}
}

void SpanningTreeProtocolInterface::SendModeNotice(const std::string &modes, const std::string &text)
{
	parameterlist p;
	p.push_back(modes);
	p.push_back(":" + text);
	Utils->DoOneToMany(ServerInstance->Config->GetSID(), "MODENOTICE", p);
}

void SpanningTreeProtocolInterface::SendSNONotice(const std::string &snomask, const std::string &text)
{
	parameterlist p;
	p.push_back(snomask);
	p.push_back(":" + text);
	Utils->DoOneToMany(ServerInstance->Config->GetSID(), "SNONOTICE", p);
}

void SpanningTreeProtocolInterface::PushToClient(User* target, const std::string &rawline)
{
	parameterlist p;
	p.push_back(target->uuid);
	p.push_back(rawline);
	Utils->DoOneToOne(ServerInstance->Config->GetSID(), "PUSH", p, target->server);
}

void SpanningTreeProtocolInterface::SendChannelPrivmsg(Channel* target, char status, const std::string &text)
{
	CUList exempt_list;
	Utils->SendChannelMessage(ServerInstance->Config->GetSID(), target, text, status, exempt_list, "PRIVMSG");
}

void SpanningTreeProtocolInterface::SendChannelNotice(Channel* target, char status, const std::string &text)
{
	CUList exempt_list;
	Utils->SendChannelMessage(ServerInstance->Config->GetSID(), target, text, status, exempt_list, "NOTICE");
}

void SpanningTreeProtocolInterface::SendUserPrivmsg(User* target, const std::string &text)
{
	parameterlist p;
	p.push_back(target->uuid);
	p.push_back(":" + text);
	Utils->DoOneToOne(ServerInstance->Config->GetSID(), "PRIVMSG", p, target->server);
}

void SpanningTreeProtocolInterface::SendUserNotice(User* target, const std::string &text)
{
	parameterlist p;
	p.push_back(target->uuid);
	p.push_back(":" + text);
	Utils->DoOneToOne(ServerInstance->Config->GetSID(), "NOTICE", p, target->server);
}

void SpanningTreeProtocolInterface::Introduce(User* user)
{
	if (user->quitting)
		return;
	if (IS_LOCAL(user))
	{
		std::deque<std::string> params;
		params.push_back(user->uuid);
		params.push_back(ConvToStr(user->age));
		params.push_back(user->nick);
		params.push_back(user->host);
		params.push_back(user->dhost);
		params.push_back(user->ident);
		params.push_back(user->GetIPString());
		params.push_back(ConvToStr(user->signon));
		params.push_back("+"+std::string(user->FormatModes(true)));
		params.push_back(":"+std::string(user->fullname));
		Utils->DoOneToMany(ServerInstance->Config->GetSID(), "UID", params);
	}

	TreeServer* SourceServer = Utils->FindServer(user->server);
	if (SourceServer)
	{
		SourceServer->SetUserCount(1); // increment by 1
	}
}
