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

#include "mode.h"
#include "channels.h"

class InspIRCd;

/** Channel mode +h
 */
class ModeChannelHalfOp : public ModeHandler
{
 private:
 public:
	ModeChannelHalfOp(InspIRCd* Instance);
	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding);
	std::string AddHalfOp(User *user,const char *dest,Channel *chan,int status);
	std::string DelHalfOp(User *user,const char *dest,Channel *chan,int status);
	ModePair ModeSet(User* source, User* dest, Channel* channel, const std::string &parameter);
	unsigned int GetPrefixRank();
	void RemoveMode(Channel* channel);
	void RemoveMode(User* user);
};

