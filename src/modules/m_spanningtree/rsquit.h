/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef __RSQUIT_H__
#define __RSQUIT_H__

/** Handle /RCONNECT
 */
class CommandRSQuit : public Command
{
        SpanningTreeUtilities* Utils;	/* Utility class */
 public:
        CommandRSQuit (Module* Callback, SpanningTreeUtilities* Util);
        CmdResult Handle (const std::vector<std::string>& parameters, User *user);
		RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters);
        void NoticeUser(User* user, const std::string &msg);
};

#endif
