/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef __LINK_H__
#define __LINK_H__

class Link : public refcountbase
{
 public:
	reference<ConfigTag> tag;
	std::string Name;
	std::string IPAddr;
	int Port;
	std::string SendPass;
	std::string RecvPass;
	std::string Fingerprint;
	std::string AllowMask;
	bool HiddenFromStats;
	std::string Hook;
	int Timeout;
	std::string Bind;
	bool Hidden;
	Link(ConfigTag* Tag) : tag(Tag) {}
};

class Autoconnect : public refcountbase
{
 public:
	reference<ConfigTag> tag;
	std::vector<std::string> servers;
	unsigned long Period;
	time_t NextConnectTime;
	/** Negative == inactive */
	int position;
	Autoconnect(ConfigTag* Tag) : tag(Tag) {}
};


#endif
