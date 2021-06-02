/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2020-2021 Sadie Powell <sadie@witchery.services>
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

typedef std::bitset<UCHAR_MAX + 1> AllowedChars;

namespace
{
	// The characters which are allowed in nicknames.
	AllowedChars allowedchars;

	// The characters which are allowed at the front of a nickname.
	AllowedChars allowedfrontchars;

	// The mapping of lower case characters to upper case characters.
	unsigned char casemap[UCHAR_MAX];

	bool IsValidNick(const std::string& nick)
	{
		if (nick.empty() || nick.length() > ServerInstance->Config->Limits.NickMax)
			return false;

		for (std::string::const_iterator iter = nick.begin(); iter != nick.end(); ++iter)
		{
			unsigned char chr = static_cast<unsigned char>(*iter);

			// Check that the character is allowed at the front of the nick.
			if (iter == nick.begin() && !allowedfrontchars[chr])
				return false;

			// Check that the character is allowed in the nick.
			if (!allowedchars[chr])
				return false;
		}

		return true;
	}
}

class ModuleCodepage
	: public Module
{
 private:
	// The character map which was set before this module was loaded.
	const unsigned char* origcasemap;

	// The name of the character map which was set before this module was loaded.
	const std::string origcasemapname;

	// The IsNick handler which was set before this module was loaded.
	const TR1NS::function<bool(const std::string&)> origisnick;

	// The character set used for the codepage.
	std::string charset;

	template <typename T>
	void RehashHashmap(T& hashmap)
	{
		T newhash(hashmap.bucket_count());
		for (typename T::const_iterator i = hashmap.begin(); i != hashmap.end(); ++i)
			newhash.insert(std::make_pair(i->first, i->second));
		hashmap.swap(newhash);
	}

	void ChangeNick(User* user, const std::string& message)
	{
		user->WriteNumeric(RPL_SAVENICK, user->uuid, message);
		user->ChangeNick(user->uuid);
	}

	void CheckDuplicateNick()
	{
		user_hash duplicates;
		const user_hash& users = ServerInstance->Users->GetUsers();
		for (user_hash::const_iterator iter = users.begin(); iter != users.end(); ++iter)
		{
			User* user = iter->second;
			if (user->nick == user->uuid)
				continue; // UUID users are always unique.

			std::pair<user_hash::iterator, bool> check = duplicates.insert(std::make_pair(user->nick, user));
			if (check.second)
				continue; // No duplicate.

			User* otheruser = check.first->second;
			if (otheruser->age < user->age)
			{
				// The other user connected first.
				ChangeNick(user, "Your nickname is no longer available.");
			}
			else if (otheruser->age > user->age)
			{
				// The other user connected last.
				ChangeNick(otheruser, "Your nickname is no longer available.");
				check.first->second = user;
			}
			else
			{
				// Both connected at the same time.
				ChangeNick(user, "Your nickname is no longer available.");
				ChangeNick(otheruser, "Your nickname is no longer available.");
				duplicates.erase(check.first);
			}
		}
	}

	void CheckInvalidNick()
	{
		const UserManager::LocalList& list = ServerInstance->Users.GetLocalUsers();
		for (UserManager::LocalList::const_iterator iter = list.begin(); iter != list.end(); ++iter)
		{
			LocalUser* user = *iter;
			if (user->nick != user->uuid && !ServerInstance->IsNick(user->nick))
				ChangeNick(user, "Your nickname is no longer valid.");
		}
	}

	void CheckRehash(unsigned char* prevmap)
	{
		if (!memcmp(prevmap, national_case_insensitive_map, UCHAR_MAX))
			return;

		RehashHashmap(ServerInstance->Users.clientlist);
		RehashHashmap(ServerInstance->Users.uuidlist);
		RehashHashmap(ServerInstance->chanlist);
	}

 public:
	ModuleCodepage()
		: origcasemap(national_case_insensitive_map)
		, origcasemapname(ServerInstance->Config->CaseMapping)
		, origisnick(ServerInstance->IsNick)
	{
	}

	~ModuleCodepage()
	{
		ServerInstance->IsNick = origisnick;
		CheckInvalidNick();

		ServerInstance->Config->CaseMapping = origcasemapname;
		national_case_insensitive_map = origcasemap;
		CheckDuplicateNick();
		CheckRehash(casemap);

		ServerInstance->ISupport.Build();
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		ConfigTag* codepagetag = ServerInstance->Config->ConfValue("codepage");

		const std::string name = codepagetag->getString("name");
		if (name.empty())
			throw ModuleException("<codepage:name> is a required field!");

		AllowedChars newallowedchars;
		AllowedChars newallowedfrontchars;
		ConfigTagList cpchars = ServerInstance->Config->ConfTags("cpchars");
		for (ConfigIter i = cpchars.first; i != cpchars.second; ++i)
		{
			ConfigTag* tag = i->second;

			unsigned char begin = tag->getUInt("begin", tag->getUInt("index", 0), 1, UCHAR_MAX);
			if (!begin)
				throw ModuleException("<cpchars> tag without index or begin specified at " + tag->getTagLocation());

			unsigned char end = tag->getUInt("end", begin, 1, UCHAR_MAX);
			if (begin > end)
				throw ModuleException("<cpchars:begin> must be lower than <cpchars:end> at " + tag->getTagLocation());

			bool front = tag->getBool("front", false);
			for (unsigned short pos = begin; pos <= end; ++pos)
			{
				if (pos == '\n' || pos == '\r' || pos == ' ')
				{
					throw ModuleException(InspIRCd::Format("<cpchars> tag contains a forbidden character: %u at %s",
						pos, tag->getTagLocation().c_str()));
				}

				if (front && (pos == ':' || isdigit(pos)))
				{
					throw ModuleException(InspIRCd::Format("<cpchars> tag contains a forbidden front character: %u at %s",
						pos, tag->getTagLocation().c_str()));
				}

				newallowedchars.set(pos);
				newallowedfrontchars.set(pos, front);
				ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Marked %u (%c) as allowed (front: %s)",
					pos, pos, front ? "yes" : "no");
			}
		}

		unsigned char newcasemap[UCHAR_MAX];
		for (size_t i = 0; i < UCHAR_MAX; ++i)
			newcasemap[i] = i;
		ConfigTagList cpcase = ServerInstance->Config->ConfTags("cpcase");
		for (ConfigIter i = cpcase.first; i != cpcase.second; ++i)
		{
			ConfigTag* tag = i->second;

			unsigned char lower = tag->getUInt("lower", 0, 1, UCHAR_MAX);
			if (!lower)
				throw ModuleException("<cpcase:lower> is required at " + tag->getTagLocation());

			unsigned char upper = tag->getUInt("upper", 0, 1, UCHAR_MAX);
			if (!upper)
				throw ModuleException("<cpcase:upper> is required at " + tag->getTagLocation());

			newcasemap[upper] = lower;
			ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Marked %u (%c) as the lower case version of %u (%c)",
				lower, lower, upper, upper);
		}

		charset = codepagetag->getString("charset");
		std::swap(allowedchars, newallowedchars);
		std::swap(allowedfrontchars, newallowedfrontchars);
		std::swap(casemap, newcasemap);

		ServerInstance->IsNick = &IsValidNick;
		CheckInvalidNick();

		ServerInstance->Config->CaseMapping = name;
		national_case_insensitive_map = casemap;
		CheckDuplicateNick();
		CheckRehash(newcasemap);

		ServerInstance->ISupport.Build();
	}

	void On005Numeric(std::map<std::string, std::string>& tokens) CXX11_OVERRIDE
	{
		if (!charset.empty())
			tokens["CHARSET"] = charset;
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		std::stringstream linkdata;

		linkdata << "front=";
		for (size_t i = 0; i < allowedfrontchars.size(); ++i)
			if (allowedfrontchars[i])
				linkdata << static_cast<unsigned char>(i);

		linkdata << "&middle=";
		for (size_t i = 0; i < allowedchars.size(); ++i)
			if (allowedchars[i])
				linkdata << static_cast<unsigned char>(i);

		linkdata << "&map=";
		for (size_t i = 0; i < sizeof(casemap); ++i)
			if (casemap[i] != i)
				linkdata << static_cast<unsigned char>(i) << casemap[i] << ',';

		return Version("Allows the server administrator to define what characters are allowed in nicknames and how characters should be compared in a case insensitive way.", VF_COMMON | VF_VENDOR, linkdata.str());
	}
};
MODULE_INIT(ModuleCodepage)
