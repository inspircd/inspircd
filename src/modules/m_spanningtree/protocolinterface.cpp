/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
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

void SpanningTreeSyncTarget::SendMetaData(Extensible* target, const std::string &extname, const std::string &extdata)
{
	User* u = IS_USER(target);
	Channel* c = IS_CHANNEL(target);
	if (u)
		ts.WriteLine(std::string(":")+ServerInstance->Config->GetSID()+" METADATA "+u->uuid+" "+extname+" :"+extdata);
	else if (c)
		ts.WriteLine(std::string(":")+ServerInstance->Config->GetSID()+" METADATA "+c->name+" "+ConvToStr(c->age)+" "+extname+" :"+extdata);
	else if (!target)
		ts.WriteLine(std::string(":")+ServerInstance->Config->GetSID()+" METADATA * "+extname+" :"+extdata);
}

void SpanningTreeSyncTarget::SendEncap(const std::string& cmd, const parameterlist &params)
{
	std::string line = ":"+ServerInstance->Config->GetSID()+" ENCAP * " + cmd;
	for(parameterlist::const_iterator i = params.begin(); i != params.end(); i++)
	{
		line.push_back(' ');
		if (i + 1 == params.end())
			line.push_back(':');
		line.append(*i);
	}
	ts.WriteLine(line);
}

void SpanningTreeSyncTarget::SendCommand(const std::string &line)
{
	ts.WriteLine(":" + ServerInstance->Config->GetSID() + " " + line);
}

void SpanningTreeProtocolInterface::GetServerList(ProtoServerList &sl)
{
	sl.clear();
	for (server_hash::iterator i = Utils->serverlist.begin(); i != Utils->serverlist.end(); i++)
	{
		ProtoServer ps;
		ps.servername = i->second->GetName();
		TreeServer* s = i->second->GetParent();
		ps.parentname = s ? s->GetName() : "";
		ps.usercount = i->second->UserCount;
		ps.gecos = i->second->GetDesc();
		ps.latencyms = i->second->rtt;
		sl.push_back(ps);
	}
}

bool SpanningTreeProtocolInterface::SendEncapsulatedData(const parameterlist &encap)
{
	if (encap[0].find('*') != std::string::npos)
	{
		Utils->DoOneToMany(ServerInstance->Config->GetSID(), "ENCAP", encap);
		return true;
	}
	return Utils->DoOneToOne(ServerInstance->Config->GetSID(), "ENCAP", encap, encap[0]);
}

void SpanningTreeProtocolInterface::SendMetaData(Extensible* target, const std::string &key, const std::string &data)
{
	parameterlist params;

	User* u = IS_USER(target);
	Channel* c = IS_CHANNEL(target);
	if (u)
		params.push_back(u->uuid);
	else if (c)
	{
		params.push_back(c->name);
		params.push_back(ConvToStr(c->age));
	}
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
	params.push_back(ConvToStr(channel->age));
	params.push_back(ConvToStr(ServerInstance->Time()));
	params.push_back(ServerInstance->Config->ServerName);
	params.push_back(":" + topic);

	Utils->DoOneToMany(ServerInstance->Config->GetSID(),"FTOPIC", params);
}

void SpanningTreeProtocolInterface::SendMode(User* src, Extensible* dest, irc::modestacker& cmodes, bool merge)
{
	irc::modestacker modes(cmodes);
	parameterlist outlist;

	User* u = IS_USER(dest);
	Channel* c = IS_CHANNEL(dest);
	if (u)
	{
		outlist.push_back(u->uuid);
		outlist.push_back("");
		while (!modes.empty())
		{
			outlist[1] = modes.popModeLine(FORMAT_NETWORK);
			Utils->DoOneToMany(src->uuid,"MODE",outlist);
		}
	}
	else if (c)
	{
		outlist.push_back(c->name);
		outlist.push_back(ConvToStr(c->age));
		outlist.push_back("");
		while (!modes.empty())
		{
			outlist[2] = modes.popModeLine(FORMAT_NETWORK);
			if(merge) outlist[2][0] = '=';
			Utils->DoOneToMany(src->uuid,"FMODE",outlist);
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
	std::string cname = target->name;
	if (status)
		cname = status + cname;
	TreeSocketSet list;
	CUList exempt_list;
	Utils->GetListOfServersForChannel(target,list,status,exempt_list);
	for (TreeSocketSet::iterator i = list.begin(); i != list.end(); i++)
	{
		(**i).WriteLine(text);
	}
}


void SpanningTreeProtocolInterface::SendChannelPrivmsg(Channel* target, char status, const std::string &text)
{
	SendChannel(target, status, ":" + ServerInstance->Config->GetSID()+" PRIVMSG "+target->name+" :"+text);
}

void SpanningTreeProtocolInterface::SendChannelNotice(Channel* target, char status, const std::string &text)
{
	SendChannel(target, status, ":" + ServerInstance->Config->GetSID()+" NOTICE "+target->name+" :"+text);
}

void SpanningTreeProtocolInterface::SendUserPrivmsg(User* target, const std::string &text)
{
	TreeServer* serv = Utils->FindServer(target->server);
	if (serv)
	{
		TreeSocket* sock = serv->GetSocket();
		if (sock)
		{
			sock->WriteLine(":" + ServerInstance->Config->GetSID() + " PRIVMSG " + target->nick + " :"+text);
		}
	}
}

void SpanningTreeProtocolInterface::SendUserNotice(User* target, const std::string &text)
{
	TreeServer* serv = Utils->FindServer(target->server);
	if (serv)
	{
		TreeSocket* sock = serv->GetSocket();
		if (sock)
		{
			sock->WriteLine(":" + ServerInstance->Config->GetSID() + " NOTICE " + target->nick + " :"+text);
		}
	}
}
