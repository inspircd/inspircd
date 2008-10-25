/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2008 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__

#include "hashcomp.h"

class InspIRCd;
class User;

typedef std::deque<std::string> parameterlist;

class ProtoServer
{
 public:
	std::string servername;
	std::string parentname;
	std::string gecos;
	unsigned int usercount;
	unsigned int opercount;
	unsigned int latencyms;
};

typedef std::list<ProtoServer> ProtoServerList;

class ProtocolInterface : public Extensible
{
 protected:
	InspIRCd* ServerInstance;
 public:
	ProtocolInterface(InspIRCd* Instance) : ServerInstance(Instance) { }
	virtual ~ProtocolInterface() { }

	/** Generate an ENCAP message.
	 * See the protocol documentation for the purpose of ENCAP.
	 * @param encap This is a list of string parameters, the first of which must be a server ID or glob matching servernames.
	 * The second must be a subcommand. All subsequent parameters are dependant on the subcommand.
	 * ENCAP (should) be used instead of creating new protocol messages for easier third party application support.
	 */
	virtual void SendEncapsulatedData(parameterlist &encap) { }

	virtual void SendMetaData(void* target, int type, const std::string &key, const std::string &data) { }

	virtual void SendTopic(Channel* channel, std::string &topic) { }

	virtual void SendMode(const std::string &target, parameterlist &modedata) { }

	virtual void SendModeStr(const std::string &target, const std::string &modeline)
	{
		/* Convenience function */
		irc::spacesepstream x(modeline);
		parameterlist n;
		std::string v;
		while (x.GetToken(v))
			n.push_back(v);
		SendMode(target, n);
	}

	virtual void SendModeNotice(const std::string &modes, const std::string &text) { }

	virtual void SendSNONotice(const std::string &snomask, const std::string &text) { }

	virtual void PushToClient(User* target, const std::string &rawline) { }

	virtual void SendChannelPrivmsg(Channel* target, char status, const std::string &text) { }

	virtual void SendChannelNotice(Channel* target, char status, const std::string &text) { }

	virtual void SendUserPrivmsg(User* target, const std::string &text) { }

	virtual void SendUserNotice(User* target, const std::string &text) { }

	virtual void GetServerList(ProtoServerList &sl) { }

	virtual void Introduce(User* u) { }
};

#endif

