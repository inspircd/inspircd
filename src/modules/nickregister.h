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

class NickRegistrationProvider : public DataProvider
{
 public:
	NickRegistrationProvider(Module* mod) : DataProvider(mod, "nickregistration") {}
	virtual std::vector<std::string> GetNicks(const std::string& account) = 0;
	virtual std::string GetOwner(const std::string& nick) = 0;
	virtual void SetOwner(const std::string& nick, const std::string& account) = 0;
};
