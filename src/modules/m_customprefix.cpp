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

#include "inspircd.h"

/* $ModDesc: Allows custom prefix modes to be created. */

class CustomPrefixMode : public PrefixModeHandler
{
 public:
	reference<ConfigTag> tag;
	int rank;
	bool depriv;
	CustomPrefixMode(Module* parent, ConfigTag* Tag)
		: PrefixModeHandler(parent, Tag->getString("name"), 0), tag(Tag)
	{
		std::string v = tag->getString("prefix");
		prefix = v.c_str()[0];
		v = tag->getString("letter");
		mode = v.c_str()[0];
		rank = tag->getInt("rank");
		levelrequired = tag->getInt("ranktoset", rank);
		depriv = tag->getBool("depriv", true);
	}

	unsigned int GetPrefixRank()
	{
		return rank;
	}

	ModResult AccessCheck(User* src, Channel*, std::string& value, bool adding)
	{
		if (!adding && src->nick == value && depriv)
			return MOD_RES_ALLOW;
		return MOD_RES_PASSTHRU;
	}

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		return MODEACTION_ALLOW;
	}
};

class ModuleCustomPrefix : public Module
{
	std::vector<CustomPrefixMode*> modes;
 public:
	ModuleCustomPrefix()
	{
	}

	void init()
	{
		ConfigTagList tags = ServerInstance->Config->ConfTags("customprefix");
		while (tags.first != tags.second)
		{
			ConfigTag* tag = tags.first->second;
			tags.first++;
			CustomPrefixMode* mh = new CustomPrefixMode(this, tag);
			modes.push_back(mh);
			if (mh->rank <= 0)
				throw ModuleException("Rank must be specified for prefix in " + tag->getTagLocation());
			try
			{
				ServerInstance->Modules->AddService(*mh);
			}
			catch (ModuleException& e)
			{
				throw ModuleException(e.err + " (while creating mode from " + tag->getTagLocation() + ")");
			}
		}
	}

	~ModuleCustomPrefix()
	{
		for (std::vector<CustomPrefixMode*>::iterator i = modes.begin(); i != modes.end(); i++)
			delete *i;
	}

	Version GetVersion()
	{
		return Version("Provides custom prefix channel modes", VF_VENDOR);
	}
};

MODULE_INIT(ModuleCustomPrefix)
