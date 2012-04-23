/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "mode.h"
#include "channels.h"

class InspIRCd;

/** Channel mode +b
 */
class ModeChannelBan : public ModeHandler
{
 private:
	BanItem b;
 public:
	ModeChannelBan();
	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding);
	std::string& AddBan(User *user,std::string& dest,Channel *chan,int status);
	std::string& DelBan(User *user,std::string& dest,Channel *chan,int status);
	void DisplayList(User* user, Channel* channel);
	void DisplayEmptyList(User* user, Channel* channel);
	void RemoveMode(User* user, irc::modestacker* stack = NULL);
	void RemoveMode(Channel* channel, irc::modestacker* stack = NULL);
};

