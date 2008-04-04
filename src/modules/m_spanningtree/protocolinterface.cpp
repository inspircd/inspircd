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

void SpanningTreeProtocolInterface::SendMode(const std::string &origin, const std::string &target, parameterlist &modedata)
{
}

void SpanningTreeProtocolInterface::SendOperNotice(const std::string &text)
{
}

void SpanningTreeProtocolInterface::SendModeNotice(const std::string &modes, const std::string &text)
{
}

void SpanningTreeProtocolInterface::SendSNONotice(const std::string &snomask, const std::string &text)
{
}

void SpanningTreeProtocolInterface::PushToClient(User* target, const std::string &rawline)
{
}

