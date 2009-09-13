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

#ifndef __RCONNECT_H__
#define __RCONNECT_H__

/** Handle /RCONNECT
 */
class CommandRConnect : public Command
{
        SpanningTreeUtilities* Utils;	/* Utility class */
 public:
        CommandRConnect (Module* Callback, SpanningTreeUtilities* Util);
        CmdResult Handle (const std::vector<std::string>& parameters, User *user);
};

#endif
