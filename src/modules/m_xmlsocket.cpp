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
#include "hashcomp.h"

/* $ModDesc: Provides XMLSocket support for clients */

class ModuleXMLSocket : public Module
{
	ConfigReader* Conf;
	std::vector<int> listenports;

 public:

	ModuleXMLSocket(InspIRCd* Me)
		: Module(Me)
	{
		OnRehash(NULL,"");
	}

	virtual void OnRehash(userrec* user, const std::string &param)
	{

		Conf = new ConfigReader(ServerInstance);

		for (unsigned int i = 0; i < listenports.size(); i++)
		{
			ServerInstance->Config->DelIOHook(listenports[i]);
		}

		listenports.clear();

		for (int i = 0; i < Conf->Enumerate("bind"); i++)
		{
			// For each <bind> tag
			std::string x = Conf->ReadValue("bind", "type", i);
			if (((x.empty()) || (x == "clients")) && (Conf->ReadFlag("bind", "xmlsocket", i)))
			{
				// Get the port we're meant to be listening on with SSL
				std::string port = Conf->ReadValue("bind", "port", i);
				irc::portparser portrange(port, false);
				long portno = -1;
				while ((portno = portrange.GetToken()))
				{
					try
					{
						if (ServerInstance->Config->AddIOHook(portno, this))
						{
							listenports.push_back(portno);
								for (size_t i = 0; i < ServerInstance->Config->ports.size(); i++)
								if (ServerInstance->Config->ports[i]->GetPort() == portno)
									ServerInstance->Config->ports[i]->SetDescription("xml");
						}
						else
						{
							ServerInstance->Log(DEFAULT, "m_xmlsocket.so: FAILED to enable XMLSocket on port %d, maybe you have another similar module loaded?", portno);
						}
					}
					catch (ModuleException &e)
					{
						ServerInstance->Log(DEFAULT, "m_xmlsocket.so: FAILED to enable XMLSocket on port %d: %s. Maybe it's already hooked by the same port on a different IP, or you have another similar module loaded?", portno, e.GetReason());
					}
				}
			}
		}

		DELETE(Conf);
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
				ServerInstance->Config->DelIOHook(listenports[i]);
				for (size_t j = 0; j < ServerInstance->Config->ports.size(); j++)
					if (ServerInstance->Config->ports[j]->GetPort() == listenports[i])
						ServerInstance->Config->ports[j]->SetDescription("plaintext");
			}
		}
	}

	virtual Version GetVersion()
	{
		return Version(1, 1, 0, 0, VF_VENDOR, API_VERSION);
	}

	void Implements(char* List)
	{
		List[I_OnUnloadModule] = List[I_OnRawSocketRead] = List[I_OnRawSocketWrite] = List[I_OnRehash] = 1;
	}

	virtual int OnRawSocketRead(int fd, char* buffer, unsigned int count, int &readresult)
	{
		userrec* user = dynamic_cast<userrec*>(ServerInstance->FindDescriptor(fd));

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
		userrec* user = dynamic_cast<userrec*>(ServerInstance->FindDescriptor(fd));

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

