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
#include "xline.h"

/* $ModDesc: Keeps a dynamic log of all XLines created, and stores them in a seperate conf file (xline.db). */

class ModuleXLineDB : public Module
{
	std::vector<XLine *> xlines;
 public:
	ModuleXLineDB(InspIRCd* Me) : Module(Me)
	{
		Implementation eventlist[] = { I_OnAddLine, I_OnDelLine };
		ServerInstance->Modules->Attach(eventlist, this, 2);
	}

	virtual ~ModuleXLineDB()
	{
	}

	/** Called whenever an xline is added by a local user.
	 * This method is triggered after the line is added.
	 * @param source The sender of the line or NULL for local server
	 * @param line The xline being added
	 */
	void OnAddLine(User* source, XLine* line)
	{
		xlines.push_back(line);

		for (std::vector<XLine *>::iterator i = xlines.begin(); i != xlines.end(); i++)
		{
			line = (*i);
			ServerInstance->WriteOpers("%s %s %s %lu %lu :%s", line->type.c_str(), line->Displayable(),
ServerInstance->Config->ServerName, line->set_time, line->duration, line->reason);
		}
	}

	/** Called whenever an xline is deleted.
	 * This method is triggered after the line is deleted.
	 * @param source The user removing the line or NULL for local server
	 * @param line the line being deleted
	 */
	void OnDelLine(User* source, XLine* line)
	{
		for (std::vector<XLine *>::iterator i = xlines.begin(); i != xlines.end(); i++)
		{
			if ((*i) == line)
			{
				xlines.erase(i);
				break;
			}
		}
	}

	virtual Version GetVersion()
	{
		return Version(1, 1, 0, 0, VF_VENDOR, API_VERSION);
	}
};

MODULE_INIT(ModuleXLineDB)

