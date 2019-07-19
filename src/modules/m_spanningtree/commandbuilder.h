/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013 Attila Molnar <attilamolnar@hush.com>
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

#include "utils.h"

class TreeServer;

class CmdBuilder
{
 protected:
	std::string content;

 public:
	CmdBuilder(const char* cmd)
		: content(1, ':')
	{
		content.append(ServerInstance->Config->GetSID());
		push(cmd);
	}

	CmdBuilder(TreeServer* src, const char* cmd)
		: content(1, ':')
	{
		content.append(src->GetID());
		push(cmd);
	}

	CmdBuilder(User* src, const char* cmd)
		: content(1, ':')
	{
		content.append(src->uuid);
		push(cmd);
	}

	CmdBuilder& push_raw(const std::string& s)
	{
		content.append(s);
		return *this;
	}

	CmdBuilder& push_raw(const char* s)
	{
		content.append(s);
		return *this;
	}

	CmdBuilder& push_raw(char c)
	{
		content.push_back(c);
		return *this;
	}

	template <typename T>
	CmdBuilder& push_raw_int(T i)
	{
		content.append(ConvToStr(i));
		return *this;
	}

	template <typename InputIterator>
	CmdBuilder& push_raw(InputIterator first, InputIterator last)
	{
		content.append(first, last);
		return *this;
	}

	CmdBuilder& push(const std::string& s)
	{
		content.push_back(' ');
		content.append(s);
		return *this;
	}

	CmdBuilder& push(const char* s)
	{
		content.push_back(' ');
		content.append(s);
		return *this;
	}

	CmdBuilder& push(char c)
	{
		content.push_back(' ');
		content.push_back(c);
		return *this;
	}

	template <typename T>
	CmdBuilder& push_int(T i)
	{
		content.push_back(' ');
		content.append(ConvToStr(i));
		return *this;
	}

	CmdBuilder& push_last(const std::string& s)
	{
		content.push_back(' ');
		content.push_back(':');
		content.append(s);
		return *this;
	}

	CmdBuilder& push_tags(const ClientProtocol::TagMap& tags)
	{
		if (!tags.empty())
		{
			char separator = '@';
			std::string taglist;
			for (ClientProtocol::TagMap::const_iterator iter = tags.begin(); iter != tags.end(); ++iter)
			{
				taglist.push_back(separator);
				separator = ';';

				taglist.append(iter->first);
				if (!iter->second.value.empty())
				{
					taglist.push_back('=');
					taglist.append(iter->second.value);
				}
			}
			taglist.push_back(' ');
			content.insert(0, taglist);
		}
		return *this;
	}

	template<typename T>
	CmdBuilder& insert(const T& cont)
	{
		for (typename T::const_iterator i = cont.begin(); i != cont.end(); ++i)
			push(*i);
		return *this;
	}

	void push_back(const std::string& s) { push(s); }

	const std::string& str() const { return content; }
	operator const std::string&() const { return str(); }

	void Broadcast() const
	{
		Utils->DoOneToMany(*this);
	}

	void Forward(TreeServer* omit) const
	{
		Utils->DoOneToAllButSender(*this, omit);
	}

	void Unicast(User* target) const
	{
		Utils->DoOneToOne(*this, target->server);
	}
};
