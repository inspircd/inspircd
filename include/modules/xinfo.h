/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013 Peter Powell <petpow@saberuk.com>
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


#pragma once

#include "inspircd.h"

enum
{
	ERR_NOSUCHXINFO = 772,
	RPL_XINFOENTRY = 773,
	RPL_XINFOEND = 774,
	RPL_XINFOTYPE = 775
};

class XInfoException : public ModuleException
{
public:
	XInfoException(const std::string& message)
		: ModuleException(message) { }
};

class CoreExport XInfoBuilder
{
private:
	std::string line;

	void Set(const std::string& key, const std::string& type, const std::string& value)
	{
		std::map<std::string, std::string>::iterator iter = Types.find(type);
		if (iter != Types.end() && iter->second != type)
			throw XInfoException("Tried to redefine type of '" + key + "' from '" + iter->second + "' to '" + type + "'!");

		line += " " + key + " " + value;
		Types[key] = type;
	}

public:
	std::vector<std::string> Lines;
	std::map<std::string, std::string> Types;

	XInfoBuilder& Bytes(const std::string& key, char* value)
	{
		Set(key, "bytes", value);
		return *this;
	}

	// TODO: remove underscore when we drop support for old GCCs.
	XInfoBuilder& Channel_(const std::string& key, Channel* value)
	{
		Set(key, "channel", value->name);
		return *this;
	}

	XInfoBuilder& Custom(const std::string& key, const std::string& type, const std::string& value)
	{
		Set(key, "inspircd.org/" + type, value);
		return *this;
	}

	XInfoBuilder& Float(const std::string& key, double value)
	{
		Set(key, "float", ConvToStr(value));
		return *this;
	}

	XInfoBuilder& Int32(const std::string& key, int32_t value)
	{
		Set(key, "int", ConvToStr(value));
		return *this;
	}

	XInfoBuilder& Int64(const std::string& key, int64_t value)
	{
		Set(key, "long", ConvToStr(value));
		return *this;
	}

	XInfoBuilder& IP(const std::string& key, const std::string& value)
	{
		// TODO: investigate accepting an irc::sockets::sockaddrs* here.
		Set(key, "ip", value);
		return *this;
	}

	XInfoBuilder& Mask(const std::string& key, const std::string& value)
	{
		// TODO: Investigate accepting a User* here.
		Set(key, "mask", value);
		return *this;
	}

	XInfoBuilder& Nick(const std::string& key, const std::string& value)
	{
		Set(key, "nick", value);
		return *this;
	}

	void Save()
	{
		this->Lines.push_back(this->line);
		this->line.clear();
	}

	// TODO: remove underscore when we drop support for old GCCs.
	XInfoBuilder& Server_(const std::string& key, Server* value)
	{
		Set(key, "server", value->GetName());
		return *this;
	}

	XInfoBuilder& String(const std::string& key, const std::string& value)
	{
		Set(key, "string", value);
		return *this;
	}

	XInfoBuilder& TimeStamp(const std::string& key, time_t value)
	{
		Set(key, "timestamp", ConvToStr(value));
		return *this;
	}

	XInfoBuilder& UInt32(const std::string& key, uint32_t value)
	{
		Set(key, "uint", ConvToStr(value));
		return *this;
	}

	XInfoBuilder& UInt64(const std::string& key, uint64_t value)
	{
		Set(key, "ulong", ConvToStr(value));
		return *this;
	}
};

class CoreExport XInfo : public DataProvider
{
	std::map<std::string, std::string> types;
public:
	XInfo(Module* Creator, const std::string& Name)
		: DataProvider(Creator, "XINFO/" + Name) { }

	virtual void Handle(User* user, XInfoBuilder& entries) = 0;
};
