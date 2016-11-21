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
#include "main.h"
#include "utils.h"
#include "treeserver.h"
#include "treesocket.h"
#include "protocolinterface.h"

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

bool SpanningTreeProtocolInterface::SendEncapsulatedData(const parameterlist &encap)
{
	if (encap[0].find_first_of("*?") != std::string::npos)
	{
		Utils->DoOneToMany(ServerInstance->Config->GetSID(), "ENCAP", encap);
		return true;
	}
	return Utils->DoOneToOne(ServerInstance->Config->GetSID(), "ENCAP", encap, encap[0]);
}

void SpanningTreeProtocolInterface::SendMetaData(Extensible* target, const std::string &key, const std::string &data)
{
	parameterlist params;

	User* u = dynamic_cast<User*>(target);
	Channel* c = dynamic_cast<Channel*>(target);
	if (u)
		params.push_back(u->uuid);
	else if (c)
		params.push_back(c->name);
	else
		params.push_back("*");

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

void SpanningTreeProtocolInterface::SendMode(const std::string &target, const parameterlist &modedata, const std::vector<TranslateType> &translate)
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
	p.push_back(":" + rawline);
	Utils->DoOneToOne(ServerInstance->Config->GetSID(), "PUSH", p, target->server);
}

void SpanningTreeProtocolInterface::SendChannel(Channel* target, char status, const std::string &text)
{
	TreeServerList list;
	CUList exempt_list;
	Utils->GetListOfServersForChannel(target,list,status,exempt_list);
	for (TreeServerList::iterator i = list.begin(); i != list.end(); i++)
	{
		TreeSocket* Sock = i->second->GetSocket();
		if (Sock)
			Sock->WriteLine(text);
	}
}


void SpanningTreeProtocolInterface::SendChannelPrivmsg(Channel* target, char status, const std::string &text)
{
	std::string cname = target->name;
	if (status)
		cname.insert(0, 1, status);

	SendChannel(target, status, ":" + ServerInstance->Config->GetSID()+" PRIVMSG "+cname+" :"+text);
}

void SpanningTreeProtocolInterface::SendChannelNotice(Channel* target, char status, const std::string &text)
{
	std::string cname = target->name;
	if (status)
		cname.insert(0, 1, status);

	SendChannel(target, status, ":" + ServerInstance->Config->GetSID()+" NOTICE "+cname+" :"+text);
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
