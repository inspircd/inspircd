/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2021 Herman <GermanAizek@yandex.ru>
 *   Copyright (C) 2019 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2013, 2015, 2017, 2020-2021 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012, 2019 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2011 jackmcbarn <jackmcbarn@inspircd.org>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2009 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2009 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2009 Craig Edwards <brain@inspircd.org>
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


/* Contains a code of Unreal IRCd + Bynets patch (https://www.unrealircd.org and https://bynets.org)
   Original patch is made by Dmitry "Killer{R}" Kononko. (http://killprog.com)
   Changed at 2008-06-15 - 2009-02-11
   by Chernov-Phoenix Alexey (Phoenix@RusNet) mailto:phoenix /email address separator/ pravmail.ru */

#include "inspircd.h"
#include <fstream>

class lwbNickHandler {
  public:
    static bool Call(const std::string&);
};

static unsigned char m_reverse_additional[256],m_additionalMB[256],
       m_additionalUtf8[256],m_additionalUtf8range[256],m_additionalUtf8interval[256];

char utf8checkrest(unsigned char* mb, unsigned char cnt) {
    for (unsigned char* tmp=mb; tmp<mb+cnt; tmp++) {
        /* & is faster! -- Phoenix (char & b11000000 == b10000000) */
        if ((*tmp & 192) != 128) {
            return -1;
        }
    }
    return cnt + 1;
}


char utf8size(unsigned char * mb) {
    if (!*mb) {
        return -1;
    }
    if (!(*mb & 128)) {
        return 1;
    }
    if ((*mb & 224) == 192) {
        return utf8checkrest(mb + 1,1);
    }
    if ((*mb & 240) == 224) {
        return utf8checkrest(mb + 1,2);
    }
    if ((*mb & 248) == 240) {
        return utf8checkrest(mb + 1,3);
    }
    return -1;
}


/* Conditions added */
bool lwbNickHandler::Call(const std::string& nick) {
    if (nick.empty() || isdigit(nick[0])) {
        return false;
    }

    const char* n = nick.c_str();
    unsigned int p = 0;
    for (const char* i = n; *i; i++, p++) {
        /* 1. Multibyte encodings support:  */
        /* 1.1. 16bit char. areas, e.g. chinese:*/

        /* if current character is the last, we DO NOT check it against multibyte table */
        /* if there are mbtable ranges, use ONLY them. No 8bit at all */
        if (i[1] && m_additionalMB[0]) {
            /* otherwise let's take a look at the current character and the following one */
            bool found = false;
            for(unsigned char* mb = m_additionalMB; (*mb)
                    && (mb < m_additionalMB + sizeof(m_additionalMB)); mb += 4) {
                if ( (i[0] >= mb[0]) && (i[0] <= mb[1]) && (i[1] >= mb[2])
                        && (i[1] <= mb[3]) ) {
                    /* multibyte range character found */
                    i++;
                    p++;
                    found = true;
                    break;
                }
            }
            if (found)
                /* next char! */
            {
                continue;
            } else
                /* there are ranges, but incorrect char (8bit?) given, sorry */
            {
                return false;
            }
        }

        /* 2. 8bit character support */
        if (((*i >= 'A') && (*i <= '}')) || m_reverse_additional[(unsigned char)*i])
            /* "A"-"}" can occur anywhere in a nickname */
        {
            continue;
        }

        if ((((*i >= '0') && (*i <= '9')) || (*i == '-')) && (i > n))
            /* "0"-"9", "-" can occur anywhere BUT the first char of a nickname */
        {
            continue;
        }

        /* 3.1. Check against a simple UTF-8 characters enumeration */
        int cursize, cursize2, ncursize = utf8size((unsigned char*)i);
        /* do check only if current multibyte character is valid UTF-8 only */
        if (ncursize != -1) {
            bool found = false;
            for (unsigned char* mb = m_additionalUtf8; (utf8size(mb) != -1)
                    && (mb < m_additionalUtf8 + sizeof(m_additionalUtf8)); mb += cursize) {
                cursize = utf8size(mb);
                /* Size differs? Pick the next! */
                if (cursize != ncursize) {
                    continue;
                }

                if (!strncmp(i, (char*)mb, cursize)) {
                    i += cursize - 1;
                    p += cursize - 1;
                    found = true;
                    break;
                }
            }
            if (found) {
                continue;
            }

            /* 3.2. Check against an UTF-8 ranges: <start character> and <length of the range>. */
            found = false;
            for (unsigned char* mb = m_additionalUtf8range; (utf8size(mb) != -1)
                    && (mb < m_additionalUtf8range + sizeof(m_additionalUtf8range));
                    mb += cursize + 1) {
                cursize = utf8size(mb);
                /* Size differs (or lengthbyte is zero)? Pick the next! */
                if ((cursize != ncursize) || (!mb[cursize])) {
                    continue;
                }

                unsigned char uright[5] = {0,0,0,0,0}, range = mb[cursize] - 1;
                strncpy((char*) uright, (char*) mb, cursize);

                for (int temp = cursize - 1; (temp >= 0) && range; --temp) {
                    /* all but the first char are 64-based */
                    if (temp) {
                        char part64 = range & 63; /* i.e. % 64 */
                        /* handle carrying over */
                        if (uright[temp] + part64 - 1 > 191) {
                            uright[temp] -= 64;
                            range += 64;
                        }
                        uright[temp] += part64;
                        range >>= 6; /* divide it on a 64 */
                    }
                    /* the first char of UTF-8 doesn't follow the rule */
                    else {
                        uright[temp] += range;
                    }
                }

                if ((strncmp(i, (char*) mb, cursize) >= 0)
                        && (strncmp(i, (char*) uright, cursize) <= 0)) {
                    i += cursize - 1;
                    p += cursize - 1;
                    found = true;
                    break;
                }
            }
            if (found) {
                continue;
            }

            /* 3.3. Check against an UTF-8 intervals: <start character> and <end character>. */
            found = false;
            for (unsigned char* mb = m_additionalUtf8interval; (utf8size(mb) != -1)
                    && (utf8size(mb+utf8size(mb)) != -1)
                    && (mb < m_additionalUtf8interval + sizeof(m_additionalUtf8interval));
                    mb += (cursize+cursize2) ) {
                cursize = utf8size(mb);
                cursize2= utf8size(mb+cursize);

                int minlen  = cursize  > ncursize ? ncursize : cursize;
                int minlen2 = cursize2 > ncursize ? ncursize : cursize2;

                unsigned char* uright = mb + cursize;

                if ((strncmp(i, (char*) mb, minlen) >= 0)
                        && (strncmp(i, (char*) uright, minlen2) <= 0)) {
                    i += cursize - 1;
                    p += cursize - 1;
                    found = true;
                    break;
                }
            }
            if (found) {
                continue;
            }
        }

        /* invalid character! abort */
        return false;
    }

    /* too long? or not -- pointer arithmetic rocks */
    return (p < ServerInstance->Config->Limits.NickMax);
}


class ModuleNationalChars : public Module {
    std::string charset;
    unsigned char m_additional[256], m_additionalUp[256], m_lower[256],
             m_upper[256];
    TR1NS::function<bool(const std::string&)> rememberer;
    bool forcequit;
    const unsigned char * lowermap_rememberer;
    std::string casemapping_rememberer;
    unsigned char prev_map[256];

    template <typename T>
    void RehashHashmap(T& hashmap) {
        T newhash(hashmap.bucket_count());
        for (typename T::const_iterator i = hashmap.begin(); i != hashmap.end(); ++i) {
            newhash.insert(std::make_pair(i->first, i->second));
        }
        hashmap.swap(newhash);
    }

    void CheckRehash() {
        // See if anything changed
        if (!memcmp(prev_map, national_case_insensitive_map, sizeof(prev_map))) {
            return;
        }

        memcpy(prev_map, national_case_insensitive_map, sizeof(prev_map));

        RehashHashmap(ServerInstance->Users.clientlist);
        RehashHashmap(ServerInstance->Users.uuidlist);
        RehashHashmap(ServerInstance->chanlist);
    }

  public:
    ModuleNationalChars()
        : rememberer(ServerInstance->IsNick)
        , lowermap_rememberer(national_case_insensitive_map)
        , casemapping_rememberer(ServerInstance->Config->CaseMapping) {
        memcpy(prev_map, national_case_insensitive_map, sizeof(prev_map));
    }

    void init() CXX11_OVERRIDE {
        memcpy(m_lower, rfc_case_insensitive_map, 256);
        national_case_insensitive_map = m_lower;

        ServerInstance->IsNick = &lwbNickHandler::Call;
    }

    void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE {
        ConfigTag* tag = ServerInstance->Config->ConfValue("nationalchars");
        charset = tag->getString("file");
        std::string casemapping = tag->getString("casemapping", FileSystem::GetFileName(charset), 1);
        if (casemapping.find(' ') != std::string::npos) {
            throw ModuleException("<nationalchars:casemapping> must not contain any spaces!");
        }
        ServerInstance->Config->CaseMapping = casemapping;
#if defined _WIN32
        if (!FileSystem::StartsWithWindowsDriveLetter(charset)) {
            charset.insert(0, "./locales/");
        }
#else
        if(charset[0] != '/') {
            charset.insert(0, "../locales/");
        }
#endif
        unsigned char* tables[8] = { m_additional, m_additionalMB, m_additionalUp, m_lower, m_upper, m_additionalUtf8, m_additionalUtf8range, m_additionalUtf8interval };
        if (!loadtables(charset, tables, 8, 5)) {
            throw ModuleException("The locale file failed to load. Check your log file for more information.");
        }
        forcequit = tag->getBool("forcequit");
        CheckForceQuit("National character set changed");
        CheckRehash();
    }

    void CheckForceQuit(const char* message) {
        if (!forcequit) {
            return;
        }

        const UserManager::LocalList& list = ServerInstance->Users.GetLocalUsers();
        for (UserManager::LocalList::const_iterator iter = list.begin();
                iter != list.end(); ) {
            /* Fix by Brain: Dont quit UID users */
            // Quitting the user removes it from the list
            User* n = *iter;
            ++iter;
            if (!isdigit(n->nick[0]) && !ServerInstance->IsNick(n->nick)) {
                ServerInstance->Users->QuitUser(n, message);
            }
        }
    }

    ~ModuleNationalChars() {
        ServerInstance->IsNick = rememberer;
        national_case_insensitive_map = lowermap_rememberer;
        ServerInstance->Config->CaseMapping = casemapping_rememberer;
        // The core rebuilds ISupport on module unload, but before the dtor.
        ServerInstance->ISupport.Build();
        CheckForceQuit("National characters module unloaded");
        CheckRehash();
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Allows the server administrator to define what characters are allowed in nicknames and channel names and how those characters should be compared in a case insensitive way.", VF_VENDOR | VF_COMMON);
    }

    /*make an array to check against it 8bit characters a bit faster. Whether allowed or uppercase (for your needs).*/
    void makereverse(unsigned char* from, unsigned char* to, unsigned int cnt) {
        memset(to, 0, cnt);
        for(unsigned char* n=from; (*n) && ((*n)<cnt) && (n<from+cnt); n++) {
            to[*n] = 1;
        }
    }

    /*so Bynets Unreal distribution stuff*/
    bool loadtables(const std::string& filename, unsigned char** tables,
                    unsigned char cnt, char faillimit) {
        std::ifstream ifs(ServerInstance->Config->Paths.PrependConfig(
                              filename).c_str());
        if (ifs.fail()) {
            ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT,
                                      "loadtables() called for missing file: %s", filename.c_str());
            return false;
        }

        for (unsigned char n=0; n< cnt; n++) {
            memset(tables[n], 0, 256);
        }

        memcpy(m_lower, rfc_case_insensitive_map, 256);

        for (unsigned char n = 0; n < cnt; n++) {
            if (loadtable(ifs, tables[n], 255) && (n < faillimit)) {
                ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT,
                                          "loadtables() called for illegal file: %s (line %d)", filename.c_str(), n+1);
                return false;
            }
        }

        makereverse(m_additional, m_reverse_additional, sizeof(m_additional));
        return true;
    }

    unsigned char symtoi(const char* t,unsigned char base)
    /* base = 16 for hexadecimal, 10 for decimal, 8 for octal ;) */
    {
        unsigned char tmp = 0, current;
        while ((*t) && (*t !=' ') && (*t != 13) && (*t != 10) && (*t != ',')) {
            tmp *= base;
            current = ascii_case_insensitive_map[(unsigned char)*t];
            if (current >= 'a') {
                current = current - 'a' + 10;
            } else {
                current = current - '0';
            }
            tmp+=current;
            t++;
        }
        return tmp;
    }

    int loadtable(std::ifstream& ifs, unsigned char* chartable,
                  unsigned int maxindex) {
        std::string buf;
        getline(ifs, buf);

        unsigned int i = 0;
        int fail = 0;

        buf.erase(buf.find_last_not_of('\n') + 1);

        if (buf[0] == '.') { /* simple plain-text string after dot */
            i = buf.size() - 1;

            if (i > (maxindex + 1)) {
                i = maxindex + 1;
            }

            memcpy(chartable, buf.c_str() + 1, i);
        } else {
            const char * p = buf.c_str();
            while (*p) {
                if (i > maxindex) {
                    fail = 1;
                    break;
                }

                if (*p != '\'') {   /* decimal or hexadecimal char code */
                    if (*p == '0') {
                        if (p[1] == 'x')
                            /* hex with the leading "0x" */
                        {
                            chartable[i] = symtoi(p + 2, 16);
                        } else {
                            chartable[i] = symtoi(p + 1, 8);
                        }
                    }
                    /* hex form */
                    else if (*p == 'x') {
                        chartable[i] = symtoi(p + 1, 16);
                    } else { /* decimal form */
                        chartable[i] = symtoi(p, 10);
                    }
                } else { /* plain-text char between '' */
                    if (*(p + 1) == '\\') {
                        chartable[i] = *(p + 2);
                        p += 3;
                    } else {
                        chartable[i] = *(p + 1);
                        p += 2;
                    }
                }
                while (*p && (*p != ',') && (*p != ' ') && (*p != 13) && (*p != 10)) {
                    p++;
                }
                while (*p && ((*p == ',') || (*p == ' ') || (*p == 13) || (*p == 10))) {
                    p++;
                }
                i++;
            }
        }
        return fail;
    }
};

MODULE_INIT(ModuleNationalChars)
