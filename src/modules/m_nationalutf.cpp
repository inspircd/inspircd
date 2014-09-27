/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2014 Googolplexed <googol@googolplexed.net>
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
#include "caller.h"

class ModuleNationalUTF : public Module,public HandlerBase1<bool, const std::string&>
{
	caller1<bool, const std::string&> rememberer;
	typedef struct Range {
		unsigned long min;
		unsigned long max;
	} Range;

	std::vector<Range> ranges;

	//Returns the next character in UTF32 encoding and advances the iterator
	//past it
	unsigned long utf8to32(std::string str,std::string::const_iterator& it)
	{

		/*
		 * UTF8 is a variable length encoding
		 * The first UTF byte determines the number of following bytes
		 * We set aside four 8-bit bitsets to hold the theoretical maximum number of bytes
		 * As the initial header byte uses a different header, we keep that aside
		 * as 'ltr'
		 */
		std::bitset<8> ltr = *it;
		std::bitset<8> sets[3];

		//Blank out the newly created bitsets
		for(int i=0; i<3; i++) sets[i].reset();
		/*
		 * As per the UTF spec, the number of initial true bits
		 * determines the number of following bytes.
		 * For each true bit, we advance the iterator forward and copy
		 * the byte into the the relevant bitset.
		 * We also clean up the initial headers of the first byte so we
		 * don't have to do this later.
		 */

		//MSB is 7, but we've already handled that in ltr, so start at 6
		//Only a maximum of 4 bits can be set, including the initial
		//Won't accidentally iterate past the
		int bytes = 0;
		for(int i=6; ltr[i]!=0 && i>3; i--)
		{
			//For true bit in the UTF header, we grab an extra byte.
			it++;
			if(it == str.end()) return 0;         //Prevent invalid header from iterating too far.
			bytes++;
			sets[6-i] = std::bitset<8>(*it);

			//Remove the UTF header of the first byte
			ltr[i] = 0;

		}
		it--;        //Set the iterator back so that the loop increments into right position;
		ltr[7] = 0;         //Remove the initial header bit
		/*
		   Here we do a basic check to ensure that the encoding is valid
		   and a client isn't sending garbage data.
		 */
		for(int i=0; i<3; i++)
		{
			//If an empty bitset is encountered, we can skip checking the rest
			//(They will all be empty too)
			if(!sets[i].any())
				break;

			if(sets[i][7] != 1 || sets[i][6] != 0)
				return false;         //Invalid UTF encoding, subsequent byte did not have correct header

			//Strip UTF header
			sets[i][7] = 0;
		}
		/*
		 * All bitsets are now in position, validated, and with headers
		 * removed.
		 */

		//Shift all bitsets left into position.
		//Aside from the initial set, each needs to be shifted
		//slightly further than normal to compansate for where the header
		//bits were.
		unsigned long l1 = ltr.to_ulong() << (24);
		unsigned long l2 = sets[0].to_ulong() << (16+2);
		unsigned long l3 = sets[1].to_ulong() << (8+4);
		unsigned long l4 = sets[2].to_ulong() << (6);

		//The above operation assumes that the UTF8 encoding used four bytes
		//If it didn't, the below shift will compensate, moving them into
		//proper position
		unsigned long utf32 = (l1+l2+l3+l4) >> ((bytes* -6)+30);
		return utf32;
	}

	bool isValidUTFChar(const unsigned long& utf32)
	{
		for(std::vector<Range>::iterator it = ranges.begin(); it != ranges.end(); ++it)
		{
			if(utf32 < (*it).max && utf32 > (*it).min)
				return true;
		}
		return false;
	}

public:
	ModuleNationalUTF()
		: rememberer(ServerInstance->IsNick) {
	}

	//Check if Nick is valid using the current codepage
	bool Call(const std::string& nick)
	{
		for(std::string::const_iterator it = nick.begin(); it != nick.end(); ++it)
		{
			//Check if character in normal range
			if ((*it >= 'A') && (*it <= '}'))
				continue;

			//Check if character is okay, but needs to be after the initial
			if ((((*it >= '0') && (*it <= '9')) || (*it == '-')) && (it != nick.begin()))
				continue;

			unsigned long utf32 = utf8to32(nick,it);
			if(isValidUTFChar(utf32))
				continue;

			return false; //Uncaught character, not valid
		}
		return true;
	}

	void init() CXX11_OVERRIDE
	{
		ServerInstance->IsNick = this;
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		ranges.clear();

		ConfigTagList chars = ServerInstance->Config->ConfTags("utfrange");
		for (ConfigIter i = chars.first; i != chars.second; ++i)
		{
			int lower = i->second->getInt("lower");
			int upper = i->second->getInt("upper");
			int value = i->second->getInt("value");
			if(value)
			{
				lower = value;
				upper = value;
			}
			if(lower && upper)
			{
				Range mrange;
				mrange.min = lower;
				mrange.max = upper;
				ranges.push_back(mrange);
			}
		}
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Provides UTF nick support", VF_VENDOR|VF_COMMON);
	}

	~ModuleNationalUTF()
	{
		ServerInstance->IsNick = rememberer;
	}

};

MODULE_INIT(ModuleNationalUTF)
