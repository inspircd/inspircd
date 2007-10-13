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

#include "inspircd.h"
#include "users.h"
#include "channels.h"
#include "modules.h"

/* $ModDesc: Provides support for RFC1413 ident lookups */

class IdentRequestSocket : public InspSocket
{
 private:
	userrec *user;
	int original_fd;
 public:
	IdentRequestSocket(InspIRCd *Server, userrec *user, int timeout, const std::string &bindip)
		: InspSocket(Server, user->GetIPString(), 113, false, timeout, bindip), user(user)
	{
		original_fd = user->GetFd();
		Instance->Log(DEBUG, "Ident request against user with fd %d", original_fd);
	}

	virtual bool OnConnected()
	{
		if (Instance->SE->GetRef(original_fd) == user)
		{
			Instance->Log(DEBUG,"Oh dear, our user has gone AWOL on fd %d", original_fd);
			return false;
		}

		/* Both sockaddr_in and sockaddr_in6 can be safely casted to sockaddr, especially since the
		 * only members we use are in a part of the struct that should always be identical (at the
		 * byte level). */

		Instance->Log(DEBUG, "Sending ident request to %s", user->GetIPString());
		
		#ifndef IPV6
		sockaddr_in laddr, raddr;
		#else
		sockaddr_in6 laddr, raddr;
		#endif
		
		socklen_t laddrsz = sizeof(laddr);
		socklen_t raddrsz = sizeof(raddr);
		
		if ((getsockname(user->GetFd(), (sockaddr*) &laddr, &laddrsz) != 0) || (getpeername(user->GetFd(), (sockaddr*) &raddr, &raddrsz) != 0))
		{
			// Error
			return false;
		}
		
		char req[32];
		
		#ifndef IPV6
		snprintf(req, sizeof(req), "%d,%d\r\n", ntohs(raddr.sin_port), ntohs(laddr.sin_port));
		#else
		snprintf(req, sizeof(req), "%d,%d\r\n", ntohs(raddr.sin6_port), ntohs(laddr.sin6_port));
		#endif
		
		this->Write(req);
		
		return true;
	}

	virtual void OnClose()
	{
		/* There used to be a check against the fd table here, to make sure the user hadn't been
		 * deleted but not yet had their socket closed or something along those lines, dated june
		 * 2006. Since we added the global cull list and such, I don't *think* that is necessary
		 * 
		 * -- YES IT IS!!!! DO NOT REMOVE IT, THIS IS WHAT THE WARNING ABOVE THE OLD CODE SAID :P
		 */
		if (Instance->SE->GetRef(original_fd) == user)
		{
			Instance->Log(DEBUG,"Oh dear, our user has gone AWOL on fd %d", original_fd);
			return;
		}

		if (*user->ident == '~' && user->GetExt("ident_socket"))
			user->WriteServ("NOTICE Auth :*** Could not find your ident, using %s instead", user->ident);

		user->Shrink("ident_socket");
		Instance->next_call = Instance->Time();
	}
	
	virtual void OnError(InspSocketError e)
	{
		if (Instance->SE->GetRef(original_fd) == user)
		{
			Instance->Log(DEBUG,"Oh dear, our user has gone AWOL on fd %d", original_fd);
			return;
		}

		// Quick check to make sure that this hasn't been sent ;)
		if (*user->ident == '~' && user->GetExt("ident_socket"))
			user->WriteServ("NOTICE Auth :*** Could not find your ident, using %s instead", user->ident);
		
		user->Shrink("ident_socket");
		Instance->next_call = Instance->Time();
	}
	
	virtual bool OnDataReady()
	{
		if (Instance->SE->GetRef(original_fd) == user)
		{
			Instance->Log(DEBUG,"Oh dear, our user has gone AWOL on fd %d", original_fd);
			return false;
		}

		char *ibuf = this->Read();
		if (!ibuf)
			return false;
		
		// We don't really need to buffer for incomplete replies here, since IDENT replies are
		// extremely short - there is *no* sane reason it'd be in more than one packet
		
		irc::sepstream sep(ibuf, ':');
		std::string token;
		for (int i = 0; sep.GetToken(token); i++)
		{
			// We only really care about the 4th portion
			if (i < 3)
				continue;
			
			char ident[IDENTMAX + 2];
			
			// Truncate the ident at any characters we don't like, skip leading spaces
			int k = 0;
			for (const char *j = token.c_str(); *j && (k < IDENTMAX + 1); j++)
			{
				if (*j == ' ')
					continue;
				
				// Rules taken from InspIRCd::IsIdent
				if (((*j >= 'A') && (*j <= '}')) || ((*j >= '0') && (*j <= '9')) || (*j == '-') || (*j == '.'))
				{
					ident[k++] = *j;
					continue;
				}
				
				break;
			}
			
			ident[k] = '\0';
			
			// Redundant check with IsIdent, in case that changes and this doesn't (paranoia!)
			if (*ident && Instance->IsIdent(ident))
			{
				strlcpy(user->ident, ident, IDENTMAX);
				user->WriteServ("NOTICE Auth :*** Found your ident: %s", user->ident);
				Instance->next_call = Instance->Time();
			}
			
			break;
		}
		
		return false;
	}
};

class ModuleIdent : public Module
{
 private:
	int RequestTimeout;
 public:
	ModuleIdent(InspIRCd *Me)
		: Module(Me)
	{
		OnRehash(NULL, "");
	}
	
	virtual ~ModuleIdent()
	{
	}
	
	virtual Version GetVersion()
	{
		return Version(1, 1, 1, 0, VF_VENDOR, API_VERSION);
	}
	
	virtual void Implements(char *List)
	{
		List[I_OnRehash] = List[I_OnUserRegister] = List[I_OnCheckReady] = List[I_OnCleanup] = List[I_OnUserDisconnect] = 1;
	}
	
	virtual void OnRehash(userrec *user, const std::string &param)
	{
		ConfigReader MyConf(ServerInstance);
		
		RequestTimeout = MyConf.ReadInteger("ident", "timeout", 0, true);
		if (!RequestTimeout)
			RequestTimeout = 5;
	}
	
	virtual int OnUserRegister(userrec *user)
	{
		/* userrec::ident is currently the username field from USER; with m_ident loaded, that
		 * should be preceded by a ~. The field is actually IDENTMAX+2 characters wide. */
		memmove(user->ident + 1, user->ident, IDENTMAX);
		user->ident[0] = '~';
		// Ensure that it is null terminated
		user->ident[IDENTMAX + 1] = '\0';
		
		user->WriteServ("NOTICE Auth :*** Looking up your ident...");
		
		// Get the IP that the user is connected to, and bind to that for the outgoing connection
		#ifndef IPV6
		sockaddr_in laddr;
		#else
		sockaddr_in6 laddr;
		#endif
		socklen_t laddrsz = sizeof(laddr);
		
		if (getsockname(user->GetFd(), (sockaddr*) &laddr, &laddrsz) != 0)
		{
			user->WriteServ("NOTICE Auth :*** Could not find your ident, using %s instead.", user->ident);
			return 0;
		}
		
		#ifndef IPV6
		const char *ip = inet_ntoa(laddr.sin_addr);
		#else
		char ip[INET6_ADDRSTRLEN + 1];
		inet_ntop(laddr.sin6_family, &laddr.sin6_addr, ip, INET6_ADDRSTRLEN);
		#endif
		
		IdentRequestSocket *isock = new IdentRequestSocket(ServerInstance, user, RequestTimeout, ip);
		if (isock->GetFd() > -1)
			user->Extend("ident_socket", isock);
		else
			if (ServerInstance->SocketCull.find(isock) == ServerInstance->SocketCull.end())
				ServerInstance->SocketCull[isock] = isock;
		return 0;
	}
	
	virtual bool OnCheckReady(userrec *user)
	{
		return (!user->GetExt("ident_socket"));
	}
	
	virtual void OnCleanup(int target_type, void *item)
	{
		if (target_type == TYPE_USER)
		{
			IdentRequestSocket *isock;
			userrec *user = (userrec*)item;
			if (user->GetExt("ident_socket", isock))
				isock->Close();
		}
	}
	
	virtual void OnUserDisconnect(userrec *user)
	{
		IdentRequestSocket *isock;
		if (user->GetExt("ident_socket", isock))
			isock->Close();
	}
};

MODULE_INIT(ModuleIdent);
