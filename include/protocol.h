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

class InspIRCd;

typedef std::deque<std::string> parameterlist;

class ProtocolInterface : public Extensible
{
 protected:
	InspIRCd* ServerInstance;
 public:
	ProtocolInterface(InspIRCd* Instance) : ServerInstance(Instance) { }
	virtual ~ProtocolInterface() { }

	virtual void SendEncapsulatedData(parameterlist &encap) { }
	virtual void SendMetaData(void* target, int type, const std::string &key, const std::string &data) { }
	virtual void SendTopic(Channel* channel, std::string &topic) { }
	virtual void SendMode(const std::string &target, parameterlist &modedata) { }
	virtual void SendOperNotice(const std::string &text) { }
	virtual void SendModeNotice(const std::string &modes, const std::string &text) { }
	virtual void SendSNONotice(const std::string &snomask, const std::string &text) { }
	virtual void PushToClient(User* target, const std::string &rawline) { }
};

#endif

