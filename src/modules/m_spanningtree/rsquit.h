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

#ifndef __RSQUIT_H__
#define __RSQUIT_H__

/** Handle /RCONNECT
 */
class cmd_rsquit : public command_t
{
        Module* Creator;		/* Creator */
        SpanningTreeUtilities* Utils;	/* Utility class */
 public:
        cmd_rsquit (InspIRCd* Instance, Module* Callback, SpanningTreeUtilities* Util);
        CmdResult Handle (const char** parameters, int pcnt, userrec *user);
        void NoticeUser(userrec* user, const std::string &msg);
};

#endif
