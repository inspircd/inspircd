/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2012 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

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
	SpanningTreeProtocolInterface(ModuleSpanningTree* mod, SpanningTreeUtilities* util, InspIRCd* Instance) : ProtocolInterface(Instance), Utils(util), Module(mod) { }
	virtual ~SpanningTreeProtocolInterface() { }

	virtual void SendEncapsulatedData(parameterlist &encap);
	virtual void SendMetaData(void* target, TargetTypeFlags type, const std::string &key, const std::string &data);
	virtual void SendTopic(Channel* channel, std::string &topic);
	virtual void SendMode(const std::string &target, const parameterlist &modedata, const std::deque<TranslateType> &types);
	virtual void SendModeNotice(const std::string &modes, const std::string &text);
	virtual void SendSNONotice(const std::string &snomask, const std::string &text);
	virtual void PushToClient(User* target, const std::string &rawline);
	virtual void SendChannelPrivmsg(Channel* target, char status, const std::string &text);
	virtual void SendChannelNotice(Channel* target, char status, const std::string &text);
	virtual void SendUserPrivmsg(User* target, const std::string &text);
	virtual void SendUserNotice(User* target, const std::string &text);
	virtual void GetServerList(ProtoServerList &sl);
	virtual void Introduce(User* u);
};

#endif

