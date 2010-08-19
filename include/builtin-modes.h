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

#include "u_listmode.h"

/** Channel mode +b
 */
class ModeChannelBan : public ListModeBase
{
 public:
	ModeChannelBan();
};

/** Channel mode +i
 */
class ModeChannelInviteOnly : public SimpleChannelModeHandler
{
 public:
	ModeChannelInviteOnly();
};

/** Channel mode +k
 */
class ModeChannelKey : public ModeHandler
{
 public:
	ModeChannelKey();
	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding);
};
/** Channel mode +l
 */
class ModeChannelLimit : public ParamChannelModeHandler
{
 public:
	ModeChannelLimit();
	bool ParamValidate(std::string& parameter);
	bool ParamOrder(const std::string&, const std::string&);
};
/** Channel mode +m
 */
class ModeChannelModerated : public SimpleChannelModeHandler
{
 public:
	ModeChannelModerated();
};
/** Channel mode +n
 */
class ModeChannelNoExternal : public SimpleChannelModeHandler
{
 public:
	ModeChannelNoExternal();
};

/** Channel mode +o
 */
class ModeChannelOp : public PrefixModeHandler
{
 private:
 public:
	ModeChannelOp();
	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding);
	unsigned int GetPrefixRank();
};

/** Channel mode +p
 */
class ModeChannelPrivate : public SimpleChannelModeHandler
{
 public:
	ModeChannelPrivate();
};
/** Channel mode +s
 */
class ModeChannelSecret : public SimpleChannelModeHandler
{
 public:
	ModeChannelSecret();
};
/** Channel mode +t
 */
class ModeChannelTopicOps : public SimpleChannelModeHandler
{
 public:
	ModeChannelTopicOps();
};

/** Channel mode +v
 */
class ModeChannelVoice : public PrefixModeHandler
{
 private:
 public:
	ModeChannelVoice();
	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding);
	unsigned int GetPrefixRank();
};

/** User mode +i
 */
class ModeUserInvisible : public SimpleUserModeHandler
{
 public:
	ModeUserInvisible();
};
/** User mode +o
 */
class ModeUserOperator : public ModeHandler
{
 public:
	ModeUserOperator();
	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding);
};
/** User mode +n
 */
class ModeUserServerNoticeMask : public ModeHandler
{
 public:
	ModeUserServerNoticeMask();
	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding);
	void OnParameterMissing(User* user, User* dest, Channel* channel, std::string& parameter);
	std::string GetUserParameter(User* user);
};
/** User mode +w
 */
class ModeUserWallops : public SimpleUserModeHandler
{
 public:
	ModeUserWallops();
};
