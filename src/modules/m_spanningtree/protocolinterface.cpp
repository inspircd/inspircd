#include "inspircd.h"
#include "m_spanningtree/main.h"
#include "m_spanningtree/utils.h"
#include "m_spanningtree/protocolinterface.h"

void SpanningTreeProtocolInterface::SendEncapsulatedData(parameterlist &encap)
{
	Utils->DoOneToMany(ServerInstance->Config->GetSID(), "ENCAP", encap);
}

void SpanningTreeProtocolInterface::SendMetaData(void* target, int type, const std::string &key, const std::string &data)
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
		case TYPE_SERVER:
			params.push_back(ServerInstance->Config->GetSID());
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

void SpanningTreeProtocolInterface::SendMode(const std::string &target, parameterlist &modedata)
{
	if (modedata.empty())
		return;

	/* Warning: in-place translation is only safe for type TR_NICK */
	for (size_t n = 0; n < modedata.size(); n++)
		ServerInstance->Parser->TranslateUIDs(TR_NICK, modedata[n], modedata[n]);

	std::string uidtarget;
	ServerInstance->Parser->TranslateUIDs(TR_NICK, target, uidtarget);
	modedata.insert(modedata.begin(), uidtarget);

	User* a = ServerInstance->FindNick(uidtarget);
	if (a)
	{
		Utils->DoOneToMany(ServerInstance->Config->GetSID(),"MODE",modedata);
		return;
	}
	else
	{
		Channel* c = ServerInstance->FindChan(target);
		if (c)
		{
			modedata.insert(modedata.begin() + 1, ConvToStr(c->age));
			Utils->DoOneToMany(ServerInstance->Config->GetSID(),"FMODE",modedata);
		}
	}
}

void SpanningTreeProtocolInterface::SendOperNotice(const std::string &text)
{
	parameterlist p;
	p.push_back(":" + text);
	Utils->DoOneToMany(ServerInstance->Config->GetSID(), "OPERNOTICE", p);
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

