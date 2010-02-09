#ifndef _SPANNINGTREE_PROTOCOL_INT_
#define _SPANNINGTREE_PROTOCOL_INT_

class SpanningTreeUtilities;
class ModuleSpanningTree;

class SpanningTreeProtocolInterface : public ProtocolInterface
{
	SpanningTreeUtilities* Utils;
	ModuleSpanningTree* Module;
	void SendChannel(Channel* target, char status, const std::string &text);
 public:
	SpanningTreeProtocolInterface(ModuleSpanningTree* mod, SpanningTreeUtilities* util) : Utils(util), Module(mod) { }
	virtual ~SpanningTreeProtocolInterface() { }

	virtual bool SendEncapsulatedData(const parameterlist &encap);
	virtual void SendMetaData(Extensible* target, const std::string &key, const std::string &data);
	virtual void SendTopic(Channel* channel, std::string &topic);
	virtual void SendMode(User*, Extensible*, irc::modestacker&);
	virtual void SendSNONotice(const std::string &snomask, const std::string &text);
	virtual void PushToClient(User* target, const std::string &rawline);
	virtual void SendChannelPrivmsg(Channel* target, char status, const std::string &text);
	virtual void SendChannelNotice(Channel* target, char status, const std::string &text);
	virtual void SendUserPrivmsg(User* target, const std::string &text);
	virtual void SendUserNotice(User* target, const std::string &text);
	virtual void GetServerList(ProtoServerList &sl);
};

#endif

