/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef __LINK_H__
#define __LINK_H__

/** The Link class might as well be a struct,
 * but this is C++ and we don't believe in structs (!).
 * It holds the entire information of one <link>
 * tag from the main config file. We maintain a list
 * of them, and populate the list on rehash/load.
 */
class Link : public classbase
{
 public:
	irc::string Name;
	std::string IPAddr;
	int Port;
	std::string SendPass;
	std::string RecvPass;
	std::string AllowMask;
	unsigned long AutoConnect;
	time_t NextConnectTime;
	bool HiddenFromStats;
	std::string FailOver;
	std::string Hook;
	int Timeout;
	std::string Bind;
	bool Hidden;
};

#endif
