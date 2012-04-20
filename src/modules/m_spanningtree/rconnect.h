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


#ifndef __RCONNECT_H__
#define __RCONNECT_H__

/** Handle /RCONNECT
 */
class CommandRConnect : public Command
{
        Module* Creator;		/* Creator */
        SpanningTreeUtilities* Utils;	/* Utility class */
 public:
        CommandRConnect (InspIRCd* Instance, Module* Callback, SpanningTreeUtilities* Util);
        CmdResult Handle (const std::vector<std::string>& parameters, User *user);
};

#endif
