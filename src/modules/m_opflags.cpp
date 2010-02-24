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

/* $ModDesc: Handles +x flag:nick channel mode */

class FlagMode : public ModeHandler
{
 public:
	SimpleExtItem<std::vector<std::string> > ext;
	FlagMode(Module* parent) : ModeHandler(parent, "opflags", 'x', PARAM_ALWAYS, MODETYPE_CHANNEL),
		ext("flaglist", parent)
	{
		fixed_letter = true;
		list = true;
		m_paramtype = TR_CUSTOM;
		levelrequired = OP_VALUE;
	}

	ModResult AccessCheck(User* src, Channel*, std::string& value, bool adding)
	{
		// TODO remove flags from self
		// TODO maximum size of flag list on a person?
		return MOD_RES_PASSTHRU;
	}

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		std::string::size_type colon = parameter.find(':');
		if (colon == std::string::npos)
			return MODEACTION_DENY;
		std::string flag = parameter.substr(0, colon);
		std::string nick = parameter.substr(colon + 1);
		dest = ServerInstance->FindNick(nick);
		if (!dest)
		{
			source->WriteNumeric(ERR_NOSUCHNICK, "%s %s :No such nick/channel",
				source->nick.c_str(), parameter.c_str());
			return MODEACTION_DENY;
		}
		Membership* memb = channel->GetUser(dest);
		if (!memb)
			return MODEACTION_DENY;
		std::vector<std::string>* ptr = ext.get(memb);
		if (adding && !ptr)
			ext.set(memb, ptr = new std::vector<std::string>);
		if (!ptr)
			return MODEACTION_ALLOW;
		for(std::vector<std::string>::iterator i = ptr->begin(); i != ptr->end(); i++)
		{
			if (*i == flag)
			{
				if (adding)
					return MODEACTION_DENY;
				else
				{
					ptr->erase(i);
					if (ptr->empty())
						ext.unset(memb);
					return MODEACTION_ALLOW;
				}
			}
		}
		if (adding)
			ptr->push_back(flag);
		return MODEACTION_ALLOW;
	}

	void PopulateChanModes(Channel* channel, irc::modestacker& stack)
	{
		const UserMembList* users = channel->GetUsers();
		for(UserMembCIter u = users->begin(); u != users->end(); u++)
		{
			std::vector<std::string>* ptr = ext.get(u->second);
			if (ptr)
			{
				for(std::vector<std::string>::iterator i = ptr->begin(); i != ptr->end(); i++)
				{
					std::string value = *i + ":" + u->first->uuid;
					stack.push(irc::modechange(id, value, true));
				}
			}
		}
	}

	void TranslateMode(std::string& value, bool adding, bool use_uid)
	{
		std::string::size_type colon = value.find(':');
		if (colon == std::string::npos)
			return;
		std::string flag = value.substr(0, colon + 1);
		std::string nick = value.substr(colon + 1);
		User* dest = ServerInstance->FindNick(nick);
		if (!dest)
			return;
		value = flag + (use_uid ? dest->uuid : dest->nick);
	}
};

class ModuleCustomPrefix : public Module
{
	FlagMode mode;
 public:
	ModuleCustomPrefix() : mode(this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(mode);
	}

	Version GetVersion()
	{
		return Version("Provides custom prefix channel modes", VF_VENDOR);
	}
};

MODULE_INIT(ModuleCustomPrefix)
