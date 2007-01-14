/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "mode.h"
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "inspircd.h"

/* $ModDesc: Allows an extended ban (+b) syntax redirecting banned users to another channel */

class BanRedirectEntry
{
 public:
	std::string targetchan;
	std::string banmask;

	BanRedirectEntry(const std::string &target = "", const std::string &mask = "")
	: targetchan(target), banmask(mask)
	{
	}
};

typedef std::vector<BanRedirectEntry> BanRedirectList;

class BanRedirect : public ModeWatcher
{
 private:
	InspIRCd* Srv;
 public:
	BanRedirect(InspIRCd* Instance)
	: ModeWatcher(Instance, 'b', MODETYPE_CHANNEL), Srv(Instance)
	{
	}

	bool BeforeMode(userrec* source, userrec* dest, chanrec* channel, std::string &param, bool adding, ModeType type)
	{
		// Srv->Log(DEBUG, "BeforeMode(%s, %s, %s, %s, %s, %s)", ((source && source->nick) ? source->nick : "NULL"), ((dest && dest->nick) ? dest->nick : "NULL"), ((channel && channel->name) ? channel->name : "NULL"), param.c_str(), (adding ? "true" : "false"), ((type == MODETYPE_CHANNEL) ? "MODETYPE_CHANNEL" : "MODETYPE_USER"));

		/* nick!ident@host -> nick!ident@host
		 * nick!ident@host#chan -> nick!ident@host#chan
		 * nick@host#chan -> nick!*@host#chan
		 * nick!ident#chan -> nick!ident@*#chan
		 * nick#chan -> nick!*@*#chan
		 */
		
		if(channel && (type == MODETYPE_CHANNEL) && param.length())
		{
			BanRedirectList* redirects;
			
			std::string mask[4];
			enum { NICK, IDENT, HOST, CHAN } current = NICK;
			std::string::iterator start_pos = param.begin();
		
			for(std::string::iterator curr = start_pos; curr != param.end(); curr++)
			{
				switch(*curr)
				{
					case '!':
						mask[current].assign(start_pos, curr);
						current = IDENT;
						start_pos = curr+1;
						break;
					case '@':
						mask[current].assign(start_pos, curr);
						current = HOST;
						start_pos = curr+1;
						break;
					case '#':
						mask[current].assign(start_pos, curr);
						current = CHAN;
						start_pos = curr;
						break;
				}
			}
				
			if(mask[current].empty())
			{
				mask[current].assign(start_pos, param.end());	
			}
				
			for(int i = 0; i < 3; i++)
			{
				if(mask[i].empty())
				{
					mask[i].assign("*");
				}
			}
				
			param.assign(mask[NICK]).append("!").append(mask[IDENT]).append("@").append(mask[HOST]);
			
			Srv->Log(DEBUG, "mask[NICK] = '%s', mask[IDENT] = '%s', mask[HOST] = '%s', mask[CHAN] = '%s'", mask[NICK].c_str(), mask[IDENT].c_str(), mask[HOST].c_str(), mask[CHAN].c_str());
	
			if(mask[CHAN].length())
			{
				if(Srv->IsChannel(mask[CHAN].c_str()))
				{
					if(irc::string(channel->name) == irc::string(mask[CHAN].c_str()))
					{
						source->WriteServ("690 %s %s :You cannot set a ban redirection to the channel the ban is on", source->nick, channel->name);
						return false;
					}
					else
					{
						if(adding)
						{
							/* It's a properly valid redirecting ban, and we're adding it */
							if(!channel->GetExt("banredirects", redirects))
							{
								redirects = new BanRedirectList;
								channel->Extend("banredirects", redirects);
							}
				
							/* Here 'param' doesn't have the channel on it yet */
							redirects->push_back(BanRedirectEntry(mask[CHAN].c_str(), param.c_str()));
							
							/* Now it does */
							param.append(mask[CHAN]);
						}
						else
						{
							/* Removing a ban, if there's no extensible there are no redirecting bans and we're fine. */
							if(channel->GetExt("banredirects", redirects))
							{
								/* But there were, so we need to remove the matching one if there is one */
					
								for(BanRedirectList::iterator redir = redirects->begin(); redir != redirects->end(); redir++)
								{
									/* Ugly as fuck */
									if((irc::string(redir->targetchan.c_str()) == irc::string(mask[CHAN].c_str())) && (irc::string(redir->banmask.c_str()) == irc::string(param.c_str())))
									{
										redirects->erase(redir);
										
										if(redirects->empty())
										{
											delete redirects;
											channel->Shrink("banredirects");
										}
										
										/* Append the channel so the default +b handler can remove the entry too */
										param.append(mask[CHAN]);
										
										break;
									}
								}
							}
						}
					}
				}
				else
				{
					source->WriteServ("403 %s %s :Invalid channel name in redirection (%s)", source->nick, channel->name, mask[CHAN].c_str());
					return false;
				}
			}
				
			Srv->Log(DEBUG, "Changed param to: %s", param.c_str());
		}
		
		return true;
	}
};

class ModuleBanRedirect : public Module
{
	BanRedirect* re;
	InspIRCd* Srv;
	
 public:
	ModuleBanRedirect(InspIRCd* Me)
	: Module::Module(Me), Srv(Me)
	{
		re = new BanRedirect(Me);
		
		if(!Srv->AddModeWatcher(re))
			throw ModuleException("Could not add mode watcher");
	}
	
	void Implements(char* List)
	{
		List[I_OnUserPreJoin] = List[I_OnChannelDelete] = List[I_OnCleanup] = 1;
	}
	
	virtual void OnChannelDelete(chanrec* chan)
	{
		OnCleanup(TYPE_CHANNEL, chan);
	}
	
	virtual void OnCleanup(int target_type, void* item)
	{
		if(target_type == TYPE_CHANNEL)
		{
			chanrec* chan = static_cast<chanrec*>(item);
			BanRedirectList* redirects;
			
			if(chan->GetExt("banredirects", redirects))
			{
				delete redirects;
				chan->Shrink("banredirects");
			}
		}
	}

	virtual int OnUserPreJoin(userrec* user, chanrec* chan, const char* cname, std::string &privs)
	{
		/* Return 1 to prevent the join, 0 to allow it */
		if (chan)
		{
			BanRedirectList* redirects;
			
			if(chan->GetExt("banredirects", redirects))
			{
				/* We actually had some ban redirects to check */
				
				std::string ipmask(user->nick);
				ipmask.append('!', 1).append(user->ident).append('@', 1).append(user->GetIPString());
				
				for(BanRedirectList::iterator redir = redirects->begin(); redir != redirects->end(); redir++)
				{
					if(Srv->MatchText(user->GetFullRealHost(), redir->banmask) || Srv->MatchText(user->GetFullHost(), redir->banmask) || Srv->MatchText(ipmask, redir->banmask))
					{
						/* tell them they're banned and are being transferred */
						Srv->Log(DEBUG, "%s matches ban on %s -- might transferred to %s", user->nick, chan->name, redir->targetchan.c_str());
						
						chanrec* destchan = Srv->FindChan(redir->targetchan);
						
						if(destchan && Srv->FindModule("m_redirect.so") && destchan->IsModeSet('L') && destchan->limit && (destchan->GetUserCounter() >= destchan->limit))
						{
							user->WriteServ("474 %s %s :Cannot join channel (You are banned)", user->nick, chan->name);
							return 1;
						}
						else
						{
							user->WriteServ("470 %s :You are banned from %s. You are being automatically redirected to %s", user->nick, chan->name, redir->targetchan.c_str());
							chanrec::JoinUser(Srv, user, redir->targetchan.c_str(), false);
							return 1;
						}
					}
				}
			}
		}
		return 0;
	}

	virtual ~ModuleBanRedirect()
	{
		Srv->Modes->DelModeWatcher(re);
		DELETE(re);
	}
	
	virtual Version GetVersion()
	{
		return Version(1, 0, 0, 0, VF_COMMON | VF_VENDOR, API_VERSION);
	}
};


class ModuleBanRedirectFactory : public ModuleFactory
{
 public:
	ModuleBanRedirectFactory()
	{
	}
	
	~ModuleBanRedirectFactory()
	{
	}
	
	virtual Module * CreateModule(InspIRCd* Me)
	{
		return new ModuleBanRedirect(Me);
	}
	
};


extern "C" void * init_module( void )
{
	return new ModuleBanRedirectFactory;
}
