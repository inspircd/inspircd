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

void SpanningTreeProtocolInterface::SendEncapsulatedData(parameterlist &encap)
{
	Utils->DoOneToMany(ServerInstance->Config->GetSID(), "ENCAP", encap);
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
	std::string cname = target->name;
	if (status)
		cname = status + cname;
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
