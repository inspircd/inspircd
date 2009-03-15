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

#include "mode.h"
#include "channels.h"

class InspIRCd;

/** Channel mode +v
 */
class ModeChannelVoice : public ModeHandler
{
 private:
 public:
	ModeChannelVoice(InspIRCd* Instance);
	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding, bool servermode);
	std::string AddVoice(User *user,const char *dest,Channel *chan,int status);
	std::string DelVoice(User *user,const char *dest,Channel *chan,int status);
	ModePair ModeSet(User* source, User* dest, Channel* channel, const std::string &parameter);
	unsigned int GetPrefixRank();
	void RemoveMode(User* user, irc::modestacker* stack = NULL);
	void RemoveMode(Channel* channel, irc::modestacker* stack = NULL);
};

