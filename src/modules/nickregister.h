/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

class NickRegisterChangeEvent : public Event
{
 public:
	const std::string nick;
	const std::string oldacct;
	const std::string newacct;
	NickRegisterChangeEvent(Module* me, const std::string& Nick, const std::string& Old, const std::string& New)
		: Event(me, "nickregistration_change"), nick(Nick), oldacct(Old), newacct(New)
	{
		Send();
	}
};


class NickRegistrationProvider : public DataProvider
{
 public:
	NickRegistrationProvider(Module* mod) : DataProvider(mod, "nickregistration") {}
	virtual std::vector<std::string> GetNicks(const std::string& account) = 0;
	virtual std::string GetOwner(const std::string& nick) = 0;
	virtual void SetOwner(const std::string& nick, const std::string& account) = 0;
};
