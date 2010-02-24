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
#include "opflags.h"

/* $ModDesc: Handles +x flag:nick channel mode */

class OpFlagProviderImpl : public OpFlagProvider
{
 public:
	SimpleExtItem<std::vector<std::string> > ext;
	OpFlagProviderImpl(Module* parent) : OpFlagProvider(parent, "opflags"), ext("flaglist", parent)
	{
	}

	const std::vector<std::string>* GetFlags(Membership* memb)
	{
		return ext.get(memb);
	}

	bool PermissionCheck(Membership* memb, const std::string& needed)
	{
		if (!memb)
			return false;
		irc::commasepstream flags(needed);
		std::string flag;
		if (flags.GetToken(flag))
		{
			ModeHandler* privmh = flag.length() == 1 ?
				ServerInstance->Modes->FindMode(flag[0], MODETYPE_CHANNEL) :
				ServerInstance->Modes->FindMode(flag);
			unsigned int neededrank = privmh ? privmh->GetPrefixRank() : INT_MAX;
			if (memb->getRank() >= neededrank)
				return true;
		}
		std::vector<std::string>* mine = ext.get(memb);
		if (!mine)
			return false;
		while (flags.GetToken(flag))
		{
			for(std::vector<std::string>::iterator i = mine->begin(); i != mine->end(); i++)
			{
				if (flag == *i)
					return true;
			}
		}
		return false;
	}
};

class FlagMode : public ModeHandler
{
 public:
	OpFlagProviderImpl prov;
	FlagMode(Module* parent) : ModeHandler(parent, "opflags", 'x', PARAM_ALWAYS, MODETYPE_CHANNEL),
		prov(parent)
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
		std::vector<std::string>* ptr = prov.ext.get(memb);
		if (adding && !ptr)
			prov.ext.set(memb, ptr = new std::vector<std::string>);
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
						prov.ext.unset(memb);
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
			std::vector<std::string>* ptr = prov.ext.get(u->second);
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
		ServerInstance->Modules->AddService(mode.prov);
		ServerInstance->Modules->AddService(mode.prov.ext);
	}

	Version GetVersion()
	{
		return Version("Provides custom prefix channel modes", VF_VENDOR);
	}
};

MODULE_INIT(ModuleCustomPrefix)
