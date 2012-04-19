/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2012 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "mode.h"

class InspIRCd;

/** User mode +n
 */
class ModeUserServerNoticeMask : public ModeHandler
{
 public:
	ModeUserServerNoticeMask(InspIRCd* Instance);
	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding, bool servermode);
	void OnParameterMissing(User* user, User* dest, Channel* channel);
	std::string GetUserParameter(User* user);
};
