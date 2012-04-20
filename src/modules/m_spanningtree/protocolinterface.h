/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
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

