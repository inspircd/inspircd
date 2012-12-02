/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2012 ChrisTX <chris@rev-crew.info>
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
#include "m_regex.h"
#include <regex>

/* $ModDesc: Regex Provider Module for std::regex Regular Expressions */
/* $ModConfig: <stdregex type="ecmascript">
 *  Specify the Regular Expression engine to use here. Valid settings are
 *  bre, ere, awk, grep, egrep, ecmascript (default if not specified)*/
/* $CompileFlags: -std=c++11 */
/* $ModDep: m_regex.h */

class StdRegexException : public ModuleException
{
public:
	StdRegexException(const std::string& rx, const std::string& error)
		: ModuleException(std::string("Error in regex ") + rx + ": " + error)
	{
	}
};

class StdRegex : public Regex
{
private:
	std::regex regexcl;
public:
	StdRegex(const std::string& rx, std::regex::flag_type fltype) : Regex(rx)
	{
		try{
			regexcl.assign(rx, fltype | std::regex::optimize);
		}
		catch(std::regex_error rxerr)
		{
			throw StdRegexException(rx, rxerr.what());
		}
	}
	
	virtual bool Matches(const std::string& text)
	{
		return std::regex_search(text, regexcl);
	}
};

class StdRegexFactory : public RegexFactory
{
 public:
	std::regex::flag_type regextype;
	StdRegexFactory(Module* m) : RegexFactory(m, "regex/stdregex") {}
	Regex* Create(const std::string& expr)
	{
		return new StdRegex(expr, regextype);
	}
};

class ModuleRegexStd : public Module
{
public:
	StdRegexFactory ref;
	ModuleRegexStd() : ref(this) {
		ServerInstance->Modules->AddService(ref);
		Implementation eventlist[] = { I_OnRehash };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
		OnRehash(NULL);
	}

	Version GetVersion()
	{
		return Version("Regex Provider Module for std::regex", VF_VENDOR);
	}
	
	void OnRehash(User* u)
	{
		ConfigTag* Conf = ServerInstance->Config->ConfValue("stdregex");
		std::string regextype = Conf->getString("type", "ecmascript");
		
		if(regextype == "bre")
			ref.regextype = std::regex::basic;
		else if(regextype == "ere")
			ref.regextype = std::regex::extended;
		else if(regextype == "awk")
			ref.regextype = std::regex::awk;
		else if(regextype == "grep")
			ref.regextype = std::regex::grep;
		else if(regextype == "egrep")
			ref.regextype = std::regex::egrep;
		else
		{
			if(regextype != "ecmascript")
				ServerInstance->SNO->WriteToSnoMask('a', "WARNING: Non-existent regex engine '%s' specified. Falling back to ECMAScript.", regextype.c_str());
			ref.regextype = std::regex::ECMAScript;
		}
	}
};

MODULE_INIT(ModuleRegexStd)
