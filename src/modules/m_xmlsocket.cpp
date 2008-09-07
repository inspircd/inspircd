/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2008 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"

/* $ModDesc: Provides XMLSocket support for clients */

class ModuleXMLSocket : public Module
{
	ConfigReader* Conf;
	std::vector<std::string> listenports;

 public:

	ModuleXMLSocket(InspIRCd* Me)
		: Module(Me)
	{
		OnRehash(NULL,"");
		Implementation eventlist[] = { I_OnUnloadModule, I_OnRawSocketRead, I_OnRawSocketWrite, I_OnRehash, I_OnHookUserIO, I_OnCleanup };
		ServerInstance->Modules->Attach(eventlist, this, 6);
	}

	bool isin(const std::string &host, int port, const std::vector<std::string> &portlist)
	{
		if (std::find(portlist.begin(), portlist.end(), "*:" + ConvToStr(port)) != portlist.end())
			return true;

		if (std::find(portlist.begin(), portlist.end(), ":" + ConvToStr(port)) != portlist.end())
			return true;

		return std::find(portlist.begin(), portlist.end(), host + ":" + ConvToStr(port)) != portlist.end();
	}

	virtual void OnRehash(User* user, const std::string &param)
	{

		Conf = new ConfigReader(ServerInstance);

		listenports.clear();

		for (int i = 0; i < Conf->Enumerate("bind"); i++)
		{
			// For each <bind> tag
			std::string x = Conf->ReadValue("bind", "type", i);
			if (((x.empty()) || (x == "clients")) && (Conf->ReadFlag("bind", "xmlsocket", i)))
			{
				// Get the port we're meant to be listening on with SSL
				std::string port = Conf->ReadValue("bind", "port", i);
				std::string addr = Conf->ReadValue("bind", "address", i);

				irc::portparser portrange(port, false);
				long portno = -1;
				while ((portno = portrange.GetToken()))
				{
					try
					{
						listenports.push_back(addr + ":" + ConvToStr(portno));
						for (size_t j = 0; j < ServerInstance->Config->ports.size(); j++)
							if ((ServerInstance->Config->ports[j]->GetPort() == portno) && (ServerInstance->Config->ports[j]->GetIP() == addr))
								ServerInstance->Config->ports[j]->SetDescription("xml");
					}
					catch (ModuleException &e)
					{
						ServerInstance->Logs->Log("m_xmlsocket",DEFAULT, "m_xmlsocket.so: FAILED to enable XMLSocket on port %ld: %s. Maybe you have another similar module loaded?", portno, e.GetReason());
					}
				}
			}
		}

		delete Conf;
	}

	virtual ~ModuleXMLSocket()
	{
	}

	virtual void OnUnloadModule(Module* mod, const std::string &name)
	{
		if (mod == this)
		{
			for(unsigned int i = 0; i < listenports.size(); i++)
			{
				for (size_t j = 0; j < ServerInstance->Config->ports.size(); j++)
					if (listenports[i] == (ServerInstance->Config->ports[j]->GetIP()+":"+ConvToStr(ServerInstance->Config->ports[j]->GetPort())))
						ServerInstance->Config->ports[j]->SetDescription("plaintext");
			}
		}
	}

	virtual void OnCleanup(int target_type, void* item)
	{
		if(target_type == TYPE_USER)
		{
			User* user = (User*)item;
			if(user->GetIOHook() == this)
				user->DelIOHook();
		}
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_VENDOR, API_VERSION);
	}

	virtual void OnHookUserIO(User* user, const std::string &targetip)
	{
		if (!user->GetIOHook() && isin(targetip,user->GetPort(),listenports))
		{
			/* Hook the user with our module */
			user->AddIOHook(this);
		}
	}

	virtual int OnRawSocketRead(int fd, char* buffer, unsigned int count, int &readresult)
	{
		User* user = dynamic_cast<User*>(ServerInstance->FindDescriptor(fd));

		if (user == NULL)
			return -1;

		int result = user->ReadData(buffer, count);

		if ((result == -1) && (errno == EAGAIN))
			return -1;
		else if (result < 1)
			return 0;

		/* XXX: The core is more than happy to split lines purely on an \n
		 * rather than a \r\n. This is good for us as it means that the size
		 * of data we are receiving is exactly the same as the size of data
		 * we asked for, and we dont need to re-implement our own socket
		 * buffering (See below)
		 */
		for (int n = 0; n < result; n++)
			if (buffer[n] == 0)
				buffer[n] = '\n';

		readresult = result;
		return result;
	}

	virtual int OnRawSocketWrite(int fd, const char* buffer, int count)
	{
		User* user = dynamic_cast<User*>(ServerInstance->FindDescriptor(fd));

		if (user == NULL)
			return -1;

		/* We want to alter the buffer, so we have to make a copy */
		char * tmpbuffer = new char[count + 1];
		memcpy(tmpbuffer, buffer, count);

		/* XXX: This will actually generate lines "looking\0\0like\0\0this"
		 * rather than lines "looking\0like\0this". This shouldnt be a problem
		 * to the client, but it saves us a TON of processing and the need
		 * to re-implement socket buffering, as the data we are sending is
		 * exactly the same length as the data we are receiving.
		 */
		for (int n = 0; n < count; n++)
			if ((tmpbuffer[n] == '\r') || (tmpbuffer[n] == '\n'))
				tmpbuffer[n] = 0;

		user->AddWriteBuf(std::string(tmpbuffer,count));
		delete [] tmpbuffer;

		return 1;
	}

};

MODULE_INIT(ModuleXMLSocket)

