/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2008 Thomas Stagner <aquanight@inspircd.org>
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


#include "m_regex.h"
#include "inspircd.h"

/* $ModDesc: Regex module using plain wildcard matching. */
/* $ModDep: m_regex.h */

class GlobRegex : public Regex
{
public:
	GlobRegex(const std::string& rx, InspIRCd* Me) : Regex(rx, Me)
	{
	}

	virtual ~GlobRegex()
	{
	}

	virtual bool Matches(const std::string& text)
	{
		return InspIRCd::Match(text, this->regex_string);
	}
};

class ModuleRegexGlob : public Module
{
public:
	ModuleRegexGlob(InspIRCd* Me) : Module(Me)
	{
		Me->Modules->PublishInterface("RegularExpression", this);
		Implementation eventlist[] = { I_OnRequest };
		Me->Modules->Attach(eventlist, this, 1);
	}

	virtual Version GetVersion()
	{
		return Version("$Id$", VF_COMMON | VF_VENDOR | VF_SERVICEPROVIDER, API_VERSION);
	}

	virtual ~ModuleRegexGlob()
	{
		ServerInstance->Modules->UnpublishInterface("RegularExpression", this);
	}

	virtual const char* OnRequest(Request* request)
	{
		if (strcmp("REGEX-NAME", request->GetId()) == 0)
		{
			return "glob";
		}
		else if (strcmp("REGEX", request->GetId()) == 0)
		{
			RegexFactoryRequest* rfr = (RegexFactoryRequest*)request;
			std::string rx = rfr->GetRegex();
			rfr->result = (Regex*)new GlobRegex(rx, ServerInstance);
			return "OK";
		}
		return NULL;
	}
};

MODULE_INIT(ModuleRegexGlob)
