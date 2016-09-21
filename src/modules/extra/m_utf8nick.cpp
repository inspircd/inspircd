/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2015 Googolplexed <googol@googolplexed.net>
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

/// $CompilerFlags: find_compiler_flags("icu-uc")
/// $LinkerFlags: find_linker_flags("icu-uc" "-licuuc -licudata")

/// $PackageInfo: require_system("darwin") icu4c pkg-config
/// $PackageInfo: require_system("ubuntu") libicu-dev pkg-config

#include "inspircd.h"
#include "caller.h"
#include <unicode/unistr.h>
#include <unicode/uniset.h>
#include <unicode/uchar.h>
#include <unicode/uscript.h>
#include <unicode/ustring.h>

#ifdef _WIN32
#pragma comment(lib, "icuuc.lib")
#endif

class ModuleUTF8 : public Module, public HandlerBase1<bool, const std::string&>
{
    caller1<bool, const std::string&> rememberer;
	USet* validCharacters;
    std::vector<UChar> unicodeStringBuf;

public:
    ModuleUTF8()
		: rememberer(ServerInstance->IsNick)
		, validCharacters(NULL)
	{
	}

    //Check if Nick is valid using the current codepage
    bool Call(const std::string& nick)
    {
		ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Checking nick \"%s\" validity", nick.c_str());

        //Nick length should be handled as per standard ASCII ? Not sure if this should be done pre or post conversion
		if ((nick.empty()) || (nick.length() > ServerInstance->Config->Limits.NickMax))
        {
            return false;
        }

        char initialCharacter = nick[0];
        if((initialCharacter <= '9' && initialCharacter >= '0') || initialCharacter == '-')
        {
            return false;
        }

        //Invalid input is replaced with U+FFFD
        if (unicodeStringBuf.size() < nick.size() + 1)
			unicodeStringBuf.resize(nick.size() + 1);
        int32_t outputLength = unicodeStringBuf.size();
        UErrorCode err = U_ZERO_ERROR;
        u_strFromUTF8(&unicodeStringBuf[0],unicodeStringBuf.size(),&outputLength,nick.c_str(),-1,&err);
	//outputLength check is only a sanity check, actual nick length check above
        if(U_FAILURE(err) || outputLength < 0 || static_cast<uint32_t>(outputLength) >= unicodeStringBuf.size())
        {
            ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Nick too long/conversion failed");
            return false;
        }

        if(!uset_containsAllCodePoints(validCharacters,&unicodeStringBuf[0],-1))
        {
            ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Nick contains invalid characters");
            return false;
        }

        return true;
    }

    void init() CXX11_OVERRIDE
    {
        ServerInstance->IsNick = this;
    }

    void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
    {
        if(validCharacters != NULL){
            uset_close(validCharacters);
        }
        validCharacters = uset_open('A','}');
        uset_addRange(validCharacters,'0','9');
        uset_add(validCharacters,'-');

        ConfigTag* tag = ServerInstance->Config->ConfValue("unicode");
        std::string inputPatternString = tag->getString("pattern");

        USet *suppliedCharacters = uset_open(2,1); //Create new empty set
        UErrorCode err = U_ZERO_ERROR;

        if(inputPatternString == "")
        {
            ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "Configuration warning: No unicode regex supplied; Falling back to traditional 7-bit character support");
        }
        else
        {
            ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "Using the following regex for Unicode support: '%s'",
                                      inputPatternString.c_str());

            int32_t patternSize = inputPatternString.length() + 1;
            UChar *patternString = (UChar *) malloc(patternSize * sizeof(UChar));
            u_strFromUTF8(patternString, patternSize, NULL, inputPatternString.c_str(), -1, &err);
            uset_applyPattern(suppliedCharacters, patternString, -1, USET_IGNORE_SPACE, &err);
            free(patternString);

            if (U_FAILURE(err)) {
                ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT,
                                          "Configuration warning: Invalid regex supplied; Falling back to traditional 7-bit character support");
            }
            else if (uset_containsRange(suppliedCharacters, 0xFFF9, 0xFFFF)) {
                ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT,
                                          "Configuration warning: Supplied regex is too open and allows the use of invalid unicode characters. Disabling for safety");
                uset_clear(suppliedCharacters);
            }
        }
        uset_addAll(validCharacters,suppliedCharacters);
        uset_close(suppliedCharacters);
        uset_compact(validCharacters);
        uset_freeze(validCharacters); //Make set immutable. Increases speed.

        int32_t size = uset_size(validCharacters);

        if(size >= 1000) {
            ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "Too many allowed characters to show");
        } else {
            for (int32_t i = 0; i < size; i++) {
                char buf[5];
                UChar buf2[5];
                UChar32 c = uset_charAt(validCharacters, i);
                u_strFromUTF32(buf2, 5, NULL, &c, 1, &err);
                u_strToUTF8(buf, 5, NULL, buf2, -1, &err);
                ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "Allowed character '%s' at codepoint U+%x", buf, c);
            }
        }

    }

    Version GetVersion() CXX11_OVERRIDE
    {
        return Version("Provides UTF8 nickname support", VF_VENDOR|VF_COMMON);
    }

    ~ModuleUTF8()
    {
        ServerInstance->IsNick = rememberer;
        if(validCharacters != NULL) {
            uset_close(validCharacters);
        }
    }
};

MODULE_INIT(ModuleUTF8)
