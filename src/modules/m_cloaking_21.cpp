/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2010 Xaquseg <xaquseg@irchighway.net>
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
#ifdef HAS_STDINT
#include <stdint.h>
#endif
#include "hash.h"

/* $ModDesc: Provides masking of user hostnames */

namespace cloak_21
{

// lowercase-only encoding similar to base64, used for hash output
static const char base36[] = "0123456789abcdefghijklmnopqrstuvwxyz";
static const char base16[] = "0123456789abcdef";

/** Handles user mode +x
 */
class CloakUser : public ModeHandler
{
 public:
	LocalStringExt ext;

	std::string debounce_uid;
	time_t debounce_ts;
	int debounce_count;

	CloakUser(Module* source)
		: ModeHandler(source, "cloak", 'x', PARAM_NONE, MODETYPE_USER),
		ext(EXTENSIBLE_USER, "cloaked_host", source), debounce_ts(0), debounce_count(0)
	{
	}

	ModeAction OnModeChange(User* source, User* dest, Channel* channel, std::string &parameter, bool adding)
	{
		LocalUser* user = IS_LOCAL(dest);

		/* For remote clients, we don't take any action, we just allow it.
		 * The local server where they are will set their cloak instead.
		 * This is fine, as we will receive it later.
		 */
		if (!user)
		{
			dest->SetMode('x',adding);
			return MODEACTION_ALLOW;
		}

		if (user->uuid == debounce_uid && debounce_ts == ServerInstance->Time())
		{
			// prevent spamming using /mode user +x-x+x-x+x-x
			if (++debounce_count > 2)
				return MODEACTION_DENY;
		}
		else
		{
			debounce_uid = user->uuid;
			debounce_count = 1;
			debounce_ts = ServerInstance->Time();
		}

		if (adding == user->IsModeSet('x'))
			return MODEACTION_DENY;

		/* don't allow this user to spam modechanges */
		if (source == dest)
			user->CommandFloodPenalty += 5000;

		if (adding)
		{
			std::string* cloak = ext.get(user);

			if (!cloak)
			{
				/* Force creation of missing cloak */
				creator->OnUserConnect(user);
				cloak = ext.get(user);
			}
			if (cloak)
			{
				user->ChangeDisplayedHost(cloak->c_str());
				user->SetMode('x',true);
				return MODEACTION_ALLOW;
			}
			else
				return MODEACTION_DENY;
		}
		else
		{
			/* User is removing the mode, so restore their real host
			 * and make it match the displayed one.
			 */
			user->SetMode('x',false);
			user->ChangeDisplayedHost(user->host.c_str());
			return MODEACTION_ALLOW;
		}
	}

};

class CommandCloak : public Command
{
 public:
	CommandCloak(Module* Creator) : Command(Creator, "CLOAK", 1)
	{
		flags_needed = 'o';
		syntax = "<host>";
	}

	CmdResult Handle(const std::vector<std::string> &parameters, User *user);
};

class ModuleCloaking : public Module
{
 public:
	CloakUser cu;
	CommandCloak ck;
	std::string prefix;
	std::string suffix;
	std::string key;
	std::vector<uint8_t> ipv4segments;
	std::vector<uint8_t> ipv4segmentlengths;
	uint8_t ipv4cloaklength;
	std::vector<uint8_t> ipv6segments;
	std::vector<uint8_t> ipv6segmentlengths;
	uint8_t ipv6cloaklength;
	int8_t hostsegments;
	uint8_t hostlength;
	bool hostheuristic;
	bool hostusesiphash;
	dynamic_reference<HashProvider> Hash;

	ModuleCloaking() : cu(this), ck(this), Hash("hash/md5")
	{
	}

	void init()
	{
		ServerInstance->Modules->AddService(cu);
		ServerInstance->Modules->AddService(ck);
		ServerInstance->Modules->AddService(cu.ext);

		Implementation eventlist[] = { I_OnCheckBan, I_OnUserConnect, I_OnChangeHost };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	
	/**
	 * This function takes a domain name string and returns the position of the dot
	 * where the hostname should be split
	 */
	std::string::size_type HostSplitPoint(const std::string &host)
	{
		int dots = 0; // dot counter
		std::string::size_type splitdot = host.length();
		
		if (hostsegments > 0) {
			for (std::string::size_type x = 0; x < host.length() - 1; ++x)
			{
				if (host[x] == '.')
				{
					splitdot = x;
					dots++;
					if(dots >= hostsegments) break;
				}
			}
			if (!hostheuristic && dots != hostsegments) return host.length(); // not enough dots, no heristic
		}
		else
		{
			for (std::string::size_type x = host.length() - 1; x > 0; --x)
			{
				if (host[x] == '.')
				{
					splitdot = x;
					dots--;
					if(dots <= hostsegments) break;
				}
			}
		}
		
		if (dots == 0 || splitdot >= host.length()) return host.length();
		
		if (hostheuristic)
		{
			std::string::size_type nextdot = splitdot + 1;
			while (nextdot < host.length())
			{
				nextdot = splitdot + 1;
				while (host[nextdot] != '.' && nextdot < host.length())
					nextdot++;
				if (nextdot - splitdot <= 4)
					splitdot = nextdot;
				else break;
			}
			
			std::string::size_type domaindot = 0;
			// pattern 1: .???
			if (host.length() > 4 && host[host.length() - 1] != '.' && host[host.length() - 2] != '.' &&
			                         host[host.length() - 3] != '.' && host[host.length() - 4] == '.')
				domaindot = host.length() - 4;
			// pattern 2: .??
			else if (host.length() > 3 && host[host.length() - 1] != '.' && host[host.length() - 2] != '.' &&
			                              host[host.length() - 3] == '.')
			{
				// pattern 2.1: .??.??
				if (host.length() > 6 && host[host.length() - 4] != '.' && host[host.length() - 5] != '.' &&
				                         host[host.length() - 6] == '.')
					domaindot = host.length() - 6;
				// pattern 2.2: .???.??
				else if (host.length() > 7 && host[host.length() - 4] != '.' && host[host.length() - 5] != '.' &&
					                          host[host.length() - 6] != '.' && host[host.length() - 7] == '.')
					domaindot = host.length() - 7;
				else
					domaindot = host.length() - 3;
			}
			if (domaindot != 0)
			{
				nextdot = domaindot - 1;
				while (host[nextdot] != '.' && nextdot > 0)
					nextdot--;
				if (nextdot != 0) domaindot = nextdot;
				if (domaindot < splitdot) splitdot = domaindot;
			}
		}
		return splitdot;
	}

	// 1 = host, 100-164 = ipv6, 200-232 = ipv4 

	/**
	 * cloaking function
	 * @param item The item to cloak (part of an IP or hostname)
	 * @param id A unique ID for this type of item (to make it unique if the item matches)
	 * @param len The length of the output. Maximum safe length is around 10 chars.
	 */
	std::string CloakSegment(const std::string& item, char id, int len)
	{
		std::string input;
		input.reserve(key.length() + 3 + item.length());
		input.push_back(id);
		input.append(key);
		input.push_back('\0'); // null does not terminate a C++ string
		input.append(item);
		
		std::string rv;
		rv.resize(len); // reserve the output length
		
		std::string binstr = Hash->sum(input).substr(0,8); // 8 bytes = 64 bits
		// endian-neutral alignment-neutral conversion, putting bytes in little-endian order
		// this basically flips the order, however is compatable with a direct cast on a little-endian platform
		// the double casts are to avoid extending the sign bit, as the chars are signed
		uint64_t bin = ((uint64_t)(uint8_t)binstr[7])   |
		           ((uint64_t)(uint8_t)binstr[6] << 8 ) |
		           ((uint64_t)(uint8_t)binstr[5] << 16) |
		           ((uint64_t)(uint8_t)binstr[4] << 24) |
		           ((uint64_t)(uint8_t)binstr[3] << 32) |
		           ((uint64_t)(uint8_t)binstr[2] << 40) |
		           ((uint64_t)(uint8_t)binstr[1] << 48) |
		           ((uint64_t)(uint8_t)binstr[0] << 56);
	

		for (int i=0; i < len; i++)
		{
			rv[i] = base36[bin % 36];
			bin /= 36;
		}
		return rv;
	}

	std::string CloakIP(const irc::sockets::sockaddrs& ip) {
		bool ipv6 = ip.sa.sa_family == AF_INET6;
		std::vector<uint8_t>* segments = ipv6? &ipv6segments : &ipv4segments;
		std::vector<uint8_t>* segmentlengths = ipv6? &ipv6segmentlengths : &ipv4segmentlengths;
		std::string rv;
		rv.reserve(ipv6? ipv6cloaklength : ipv4cloaklength);
		
		for (std::vector<uint8_t>::size_type i = 0; i < segments->size(); i++)
		{
			irc::sockets::cidr_mask mask(ip, segments->at(i));
			std::string bitstr((char*)mask.bits, ipv6?16:4);
			rv.append(CloakSegment(bitstr, (ipv6?100:200)+segments->at(i), segmentlengths->at(i)));
			if(i != segments->size() - 1) rv.push_back('.');
		}
		return rv;
	}
	
	ModResult OnCheckBan(User* user, Channel* chan, const std::string& mask)
	{
		LocalUser* lu = IS_LOCAL(user);
		if (!lu)
			return MOD_RES_PASSTHRU;

		OnUserConnect(lu);
		std::string* cloak = cu.ext.get(user);
		/* Check if they have a cloaked host, but are not using it */
		if (cloak && *cloak != user->dhost)
		{
			char cmask[MAXBUF];
			snprintf(cmask, MAXBUF, "%s!%s@%s", user->nick.c_str(), user->ident.c_str(), cloak->c_str());
			if (InspIRCd::Match(cmask,mask))
				return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}

	void Prioritize()
	{
		/* Needs to be after m_banexception etc. */
		ServerInstance->Modules->SetPriority(this, I_OnCheckBan, PRIORITY_LAST);
	}

	// this unsets umode +x on every host change. If we are actually doing a +x
	// mode change, we will call SetMode back to true AFTER the host change is done.
	void OnChangeHost(User* u, const std::string& host)
	{
		if(u->IsModeSet('x'))
		{
			u->SetMode('x', false);
			u->WriteServ("MODE %s -x", u->nick.c_str());
		}
	}

	~ModuleCloaking()
	{
	}

	Version GetVersion()
	{
		std::string testcloak = "broken";
		if (Hash)
		{
			testcloak = prefix + CloakSegment("*", 4, 8) + suffix;
		}
		return Version("Provides masking of user hostnames", VF_COMMON|VF_VENDOR, testcloak);
	}

	void ReadConfig(ConfigReadStatus& stat)
	{
		ConfigTag* tag = ServerInstance->Config->GetTag("cloak");
		prefix = tag->getString("prefix");
		suffix = tag->getString("suffix", ".IP");
		
		key = tag->getString("key");
		if (key.empty() || key == "secret")
			throw ModuleException("You have not defined cloak keys for m_cloaking. Define <cloak:key> as a network-wide secret.");
		
		long hostsegments_l = tag->getInt("hostsegments", 3);
		if (hostsegments_l < -64 || hostsegments_l > 64)
			throw ModuleException("<cloak:hostsegments> must be between -64 and 64 (inclusive)");
		hostsegments = (int8_t)hostsegments_l;
		
		long hostlength_l = tag->getInt("hostlength", 8);
		if (hostlength_l < 1 || hostlength_l > 8)
			throw ModuleException("<cloak:hostlength> must be between 1 and 8 (inclusive)");
		hostlength = (int8_t)hostlength_l;
		
		hostheuristic = tag->getBool("hostheuristic", false);
		hostusesiphash = tag->getBool("hostusesiphash", true);
		
		std::string segposstr, seglenstr;
		long segpos, seglen;
		
		//ipv4 segments
		irc::spacesepstream ipv4segmentsStream(tag->getString("ipv4segments", "24 16"));
		irc::spacesepstream ipv4segmentlengthsStream(tag->getString("ipv4segmentlengths", ""));
		
		std::vector<uint8_t> newipv4segments;
		std::vector<uint8_t> newipv4segmentlengths;
		
		while (ipv4segmentsStream.GetToken(segposstr))
		{
			segpos = strtol(segposstr.c_str(), NULL, 10);
			
			if (segpos < 0 || segpos > 31)
				throw ModuleException("All values in <cloak:ipv4segments> must be between 0 and 31 (inclusive)");
			if (!newipv4segments.empty() && newipv4segments.back() <= segpos)
				throw ModuleException("All values in <cloak:ipv4segments> must be less than the previous entry");
				
			if (ipv4segmentlengthsStream.GetToken(seglenstr)) {
				seglen = strtol(seglenstr.c_str(), NULL, 10);
				if(seglen < 1 || seglen > 8)
					throw ModuleException("All values in <cloak:ipv4segmentlengths> must be between 1 and 8 (inclusive)");
			}
			else seglen = 3;
			
			newipv4segments.push_back((uint8_t) segpos);
			newipv4segmentlengths.push_back((uint8_t) seglen);
		}
		
		if (newipv4segments.back() % 8 != 0)
			throw ModuleException("The last value in <cloak:ipv4segments> must be a multiple of 8");
		
		uint8_t newipv4cloaklength = 0;
		for (std::vector<uint8_t>::size_type i = 0; i < newipv4segmentlengths.size(); i++)
		{
			newipv4cloaklength += newipv4segmentlengths[i] + 1;
		}
		newipv4cloaklength--;
		
		if (prefix.length() + newipv4cloaklength + 1 + (newipv4segments.back() / 8) * 4 + suffix.length() > 64)
			throw ModuleException("The total length of IPv4 cloaks is above the max allowed host length");
		
		ipv4segments.swap(newipv4segments);
		ipv4segmentlengths.swap(newipv4segmentlengths);
		ipv4cloaklength = newipv4cloaklength;
		
		//ipv6 segments
		
		irc::spacesepstream ipv6segmentsStream(tag->getString("ipv6segments", "64 48 32"));
		irc::spacesepstream ipv6segmentlengthsStream(tag->getString("ipv6segmentlengths", ""));
		
		std::vector<uint8_t> newipv6segments;
		std::vector<uint8_t> newipv6segmentlengths;
		
		while (ipv6segmentsStream.GetToken(segposstr))
		{
			segpos = strtol(segposstr.c_str(), NULL, 10);
			if (segpos < 0 || segpos > 127)
				throw ModuleException("All values in <cloak:ipv6segments> must be between 0 and 127 (inclusive)");
			if (!newipv6segments.empty() && newipv6segments.back() <= segpos)
				throw ModuleException("All values in <cloak:ipv6segments> must be less than the previous entry");
			
			if (ipv6segmentlengthsStream.GetToken(seglenstr)) {
				seglen = strtol(seglenstr.c_str(), NULL, 10);
				if(seglen < 1 || seglen > 8)
					throw ModuleException("All values in <cloak:ipv6segmentlengths> must be between 1 and 8 (inclusive)");
			}
			else
			{
				long segsize = (newipv6segments.empty()? 128 : newipv6segments.back()) - segpos;
				seglen = (segsize >= 64)? 6 : 4;
			}
			
			newipv6segments.push_back((uint8_t) segpos);
			newipv6segmentlengths.push_back((uint8_t) seglen);
		}
		
		if (newipv6segments.back() % 4 != 0)
			throw ModuleException("The last value in <cloak:ipv6segments> must be a multiple of 4");
		
		uint8_t newipv6cloaklength = 0;
		for(std::vector<uint8_t>::size_type i = 0; i < newipv6segmentlengths.size(); i++)
		{
			newipv6cloaklength += newipv6segmentlengths[i] + 1;
		}
		newipv6cloaklength--;
		
		if (prefix.length() + newipv6cloaklength + 1 + newipv6segments.back() / 4 + suffix.length() > 64)
			throw ModuleException("The total length of IPv6 cloaks is above the max allowed host length");
		
		ipv6segments.swap(newipv6segments);
		ipv6segmentlengths.swap(newipv6segmentlengths);
		ipv6cloaklength = newipv6cloaklength;
		
		std::string hashname = tag->getString("hash", "md5");
		
		Hash.SetProvider("hash/" + hashname);
		if(!Hash)
			throw ModuleException("<cloak:hash> set to a hash function that could not be found");
	}

	std::string GenCloak(const irc::sockets::sockaddrs& ip, const std::string& ipstr, const std::string& host)
	{
		if (ipstr != host && hostsegments != 0) // we have a host and host cloaking is enabled
		{
			std::string::size_type splitpoint = HostSplitPoint(host);
			if (hostusesiphash)
			{
				bool ipv6 = ip.sa.sa_family == AF_INET6;
				if (prefix.length() + (ipv6? ipv6cloaklength: ipv4cloaklength) + (host.length() - splitpoint) <= 64)
					return prefix + CloakIP(ip) + host.substr(splitpoint);
			}
			else
			{
				if (prefix.length() + hostlength + (host.length() - splitpoint) <= 64)
					return prefix + CloakSegment(host, 1, hostlength) + host.substr(splitpoint);
			}
		}
		// either we're not doing host cloaking, or the host cloak would be too long
		if (ip.sa.sa_family == AF_INET6)
		{
			if (ipv6segments.back() == 0)
				return prefix + CloakIP(ip) + suffix;
			else
			{
				std::string chost;
				chost.reserve(prefix.length() + ipv6cloaklength + 1 + ipv6segments.back() / 4 + suffix.length());
				chost.append(prefix);
				chost.append(CloakIP(ip));
				chost.push_back('.');
				int8_t curbyte = ipv6segments.back() / 8;
				if (ipv6segments.back() % 8 != 0)
				{
					chost.push_back(base16[ip.in6.sin6_addr.s6_addr[curbyte] / 16]);
					curbyte--;
				}
				for(;curbyte >= 0; curbyte--)
				{
					chost.push_back(base16[ip.in6.sin6_addr.s6_addr[curbyte] % 16]);
					chost.push_back(base16[ip.in6.sin6_addr.s6_addr[curbyte] / 16]);
				}
				chost.append(suffix);
				return chost;
			}
		}
		else
		{
			if (ipv4segments.back() == 0)
				return prefix + CloakIP(ip) + suffix;
			else
			{
				const uint8_t* ip4 = (const uint8_t*)&ip.in4.sin_addr;
				char buf[65];
				if (ipv4segments.back() == 8)
					snprintf(buf, 65, "%s%s.%d%s", prefix.c_str(), CloakIP(ip).c_str(), ip4[0], suffix.c_str());
				else if(ipv4segments.back() == 16)
					snprintf(buf, 65, "%s%s.%d.%d%s", prefix.c_str(), CloakIP(ip).c_str(), ip4[1], ip4[0], suffix.c_str());
				else // must be 24
					snprintf(buf, 65, "%s%s.%d.%d.%d%s", prefix.c_str(), CloakIP(ip).c_str(), ip4[2], ip4[1], ip4[0], suffix.c_str());
				return std::string(buf);
			}
		}
		// unreachable point, all paths above return
	}

	void OnUserConnect(LocalUser* dest)
	{
		std::string* cloak = cu.ext.get(dest);
		if (cloak)
			return;

		cu.ext.set(dest, GenCloak(dest->client_sa, dest->GetIPString(), dest->host));
	}
};

CmdResult CommandCloak::Handle(const std::vector<std::string> &parameters, User *user)
{
	ModuleCloaking* mod = (ModuleCloaking*)(Module*)creator;
	irc::sockets::sockaddrs sa;
	std::string cloak;

	if (irc::sockets::aptosa(parameters[0], 0, sa))
		cloak = mod->GenCloak(sa, parameters[0], parameters[0]);
	else
		cloak = mod->GenCloak(sa, "", parameters[0]);

	user->WriteServ("NOTICE %s :*** Cloak for %s is %s", user->nick.c_str(), parameters[0].c_str(), cloak.c_str());

	return CMD_SUCCESS;
}

}

using cloak_21::ModuleCloaking;

MODULE_INIT(ModuleCloaking)
