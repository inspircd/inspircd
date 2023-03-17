/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2018 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013-2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2013 Daniel Vassdal <shutter@canternet.org>
 *   Copyright (C) 2013 Adam <Adam@anope.org>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Craig Edwards <brain@inspircd.org>
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


#include "inspircd.h"

void SnomaskManager::FlushSnotices() {
    for (int i=0; i < 26; i++) {
        masks[i].Flush();
    }
}

void SnomaskManager::EnableSnomask(char letter, const std::string &type) {
    if (letter >= 'a' && letter <= 'z') {
        masks[letter - 'a'].Description = type;
    }
}

void SnomaskManager::WriteToSnoMask(char letter, const std::string &text) {
    if (letter >= 'a' && letter <= 'z') {
        masks[letter - 'a'].SendMessage(text, letter);
    }
    if (letter >= 'A' && letter <= 'Z') {
        masks[letter - 'A'].SendMessage(text, letter);
    }
}

void SnomaskManager::WriteGlobalSno(char letter, const std::string& text) {
    WriteToSnoMask(letter, text);
    letter = toupper(letter);
    ServerInstance->PI->SendSNONotice(letter, text);
}

void SnomaskManager::WriteToSnoMask(char letter, const char* text, ...) {
    std::string textbuffer;
    VAFORMAT(textbuffer, text, text);
    this->WriteToSnoMask(letter, textbuffer);
}

void SnomaskManager::WriteGlobalSno(char letter, const char* text, ...) {
    std::string textbuffer;
    VAFORMAT(textbuffer, text, text);
    this->WriteGlobalSno(letter, textbuffer);
}

SnomaskManager::SnomaskManager() {
    EnableSnomask('c',"CONNECT");           /* Local connect notices */
    EnableSnomask('q',"QUIT");          /* Local quit notices */
    EnableSnomask('k',"KILL");          /* Kill notices */
    EnableSnomask('o',"OPER");          /* Oper up/down notices */
    EnableSnomask('a',
                  "ANNOUNCEMENT");      /* formerly WriteOpers() - generic notices to all opers */
    EnableSnomask('x',"XLINE");         /* X-line notices (G/Z/Q/K/E/R/SHUN/CBan) */
    EnableSnomask('t',"STATS");         /* Local or remote stats request */
}

bool SnomaskManager::IsSnomaskUsable(char ch) const {
    return ((isalpha(ch)) && (!masks[tolower(ch) - 'a'].Description.empty()));
}

Snomask::Snomask()
    : Count(0) {
}

void Snomask::SendMessage(const std::string& message, char letter) {
    if ((!ServerInstance->Config->NoSnoticeStack) && (message == LastMessage)
            && (letter == LastLetter)) {
        Count++;
        return;
    }

    this->Flush();

    std::string desc = GetDescription(letter);
    ModResult MOD_RESULT;
    FIRST_MOD_RESULT(OnSendSnotice, MOD_RESULT, (letter, desc, message));
    if (MOD_RESULT == MOD_RES_DENY) {
        return;
    }

    Snomask::Send(letter, desc, message);
    LastMessage = message;
    LastLetter = letter;
    Count++;
}

void Snomask::Flush() {
    if (Count > 1) {
        std::string desc = GetDescription(LastLetter);
        std::string msg = "(last message repeated " + ConvToStr(Count) + " times)";

        FOREACH_MOD(OnSendSnotice, (LastLetter, desc, msg));
        Snomask::Send(LastLetter, desc, msg);
    }

    LastMessage.clear();
    Count = 0;
}

void Snomask::Send(char letter, const std::string& desc,
                   const std::string& msg) {
    ServerInstance->Logs->Log(desc, LOG_DEFAULT, msg);
    const std::string finalmsg = InspIRCd::Format("*** %s: %s", desc.c_str(),
                                 msg.c_str());

    /* Only opers can receive snotices, so we iterate the oper list */
    const UserManager::OperList& opers = ServerInstance->Users->all_opers;
    for (UserManager::OperList::const_iterator i = opers.begin(); i != opers.end();
            ++i) {
        User* user = *i;
        // IsNoticeMaskSet() returns false for opers who aren't +s, no need to check for it separately
        if (IS_LOCAL(user) && user->IsNoticeMaskSet(letter)) {
            user->WriteNotice(finalmsg);
        }
    }
}

std::string Snomask::GetDescription(char letter) const {
    std::string ret;
    if (isupper(letter)) {
        ret = "REMOTE";
    }
    if (!Description.empty()) {
        ret += Description;
    } else {
        ret += std::string("SNO-") + (char)tolower(letter);
    }
    return ret;
}
