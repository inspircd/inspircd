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
	bool hide;
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

	void TranslateMode(std::string& value, bool adding, SerializeFormat format)
	{
		if (format == FORMAT_PERSIST || (hide && format == FORMAT_USER))
		{
			value.clear();
			return;
		}

		std::string::size_type colon = value.find(':');
		if (colon == std::string::npos)
			return;
		std::string flag = value.substr(0, colon + 1);
		std::string nick = value.substr(colon + 1);
		User* dest = ServerInstance->FindNick(nick);
		if (!dest)
			return;
		value = flag + (format == FORMAT_USER ? dest->nick : dest->uuid);
	}
};

class FlagCmd : public Command
{
 public:
	FlagMode mode;
	FlagCmd(Module* parent) : Command(parent, "OPFLAGS", 3), mode(parent)
	{
		flags_needed = FLAG_SERVERONLY; // ?
	}

	CmdResult Handle(const std::vector<std::string> &parameters, User *src)
	{
		Channel* chan = ServerInstance->FindChan(parameters[0]);
		User* user = ServerInstance->FindNick(parameters[1]);

		if (!user || !chan)
			return CMD_FAILURE;

		Membership* memb = chan->GetUser(user);
		if (!memb)
			return CMD_FAILURE;
		std::vector<std::string>* ptr = mode.prov.ext.get(memb);
		if (!ptr)
			mode.prov.ext.set(memb, ptr = new std::vector<std::string>);

		bool adding = true;
		irc::commasepstream flags(parameters[2]);
		std::string flag;
		while (flags.GetToken(flag))
		{
			if (flag[0] == '=')
			{
				ptr->clear();
				flag = flag.substr(1);
			}
			else if (flag[0] == '+' || flag[0] == '-')
			{
				adding = (flag[0] == '+');
				flag = flag.substr(1);
			}
			if (flag.empty())
				continue;
			for(std::vector<std::string>::iterator i = ptr->begin(); i != ptr->end(); i++)
			{
				if (*i == flag)
				{
					if (!adding)
						ptr->erase(i);
					goto found;
				}
			}
			if (adding)
				ptr->push_back(flag);
found:		;
		}
		if (ptr->empty())
			mode.prov.ext.unset(memb);
		return CMD_SUCCESS;
	}

	RouteDescriptor GetRouting(User* user, const std::vector<std::string>& parameters)
	{
		return ROUTE_OPT_BCAST;
	}
};

class ModuleOpFlags : public Module
{
	FlagCmd cmd;
 public:
	ModuleOpFlags() : cmd(this)
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(cmd);
		ServerInstance->Modules->AddService(cmd.mode);
		ServerInstance->Modules->AddService(cmd.mode.prov);
		ServerInstance->Modules->AddService(cmd.mode.prov.ext);
		OnRehash(NULL);

		Implementation eventlist[] = { I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, 1);
	}

	void OnRehash(User*)
	{
		ConfigTag* tag = ServerInstance->Config->ConfValue("opflags");
		cmd.mode.hide = tag->getBool("hidden");
	}

	Version GetVersion()
	{
		return Version("Provides per-channel access flags", VF_VENDOR);
	}
};

MODULE_INIT(ModuleOpFlags)
