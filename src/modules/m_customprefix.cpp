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

class CustomPrefixMode : public ModeHandler
{
 public:
	reference<ConfigTag> tag;
	int rank;
	bool depriv;
	CustomPrefixMode(Module* parent, ConfigTag* Tag)
		: ModeHandler(parent, Tag->getString("name"), 0, PARAM_ALWAYS, MODETYPE_CHANNEL), tag(Tag)
	{
		list = true;
		m_paramtype = TR_NICK;
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

	void RemoveMode(Channel* channel, irc::modestacker* stack)
	{
		const UserMembList* cl = channel->GetUsers();
		std::vector<std::string> mode_junk;
		mode_junk.push_back(channel->name);
		irc::modestacker modestack(false);
		std::deque<std::string> stackresult;

		for (UserMembCIter i = cl->begin(); i != cl->end(); i++)
		{
			if (i->second->hasMode(mode))
			{
				if (stack)
					stack->Push(this->GetModeChar(), i->first->nick);
				else
					modestack.Push(this->GetModeChar(), i->first->nick);
			}
		}

		if (stack)
			return;

		while (modestack.GetStackedLine(stackresult))
		{
			mode_junk.insert(mode_junk.end(), stackresult.begin(), stackresult.end());
			ServerInstance->SendMode(mode_junk, ServerInstance->FakeClient);
			mode_junk.erase(mode_junk.begin() + 1, mode_junk.end());
		}
	}

	void RemoveMode(User* user, irc::modestacker* stack)
	{
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
				throw ModuleException("Rank must be specified for prefix at " + tag->getTagLocation());
			if (!isalpha(mh->GetModeChar()))
				throw ModuleException("Mode must be a letter for prefix at " + tag->getTagLocation());
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
