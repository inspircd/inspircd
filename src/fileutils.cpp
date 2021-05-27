/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2019-2020 Sadie Powell <sadie@witchery.services>
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


#include "inspircd.h"

#include <fstream>

#ifndef _WIN32
# include <dirent.h>
#endif

FilePosition::FilePosition(const std::string& Name, unsigned long Line, unsigned long Column)
	: name(Name)
	, line(Line)
	, column(Column)
{
}

std::string FilePosition::str() const
{
	return name + ":" + ConvToStr(line) + ":" + ConvToStr(column);
}

FileReader::FileReader(const std::string& filename)
{
	Load(filename);
}

void FileReader::Load(const std::string& filename)
{
	// If the file is stored in the file cache then we used that version instead.
	ConfigFileCache::const_iterator it = ServerInstance->Config->Files.find(filename);
	if (it != ServerInstance->Config->Files.end())
	{
		this->lines = it->second;
	}
	else
	{
		std::ifstream stream(ServerInstance->Config->Paths.PrependConfig(filename));
		if (!stream.is_open())
			throw CoreException(filename + " does not exist or is not readable!");

		lines.clear();
		std::string line;
		while (std::getline(stream, line))
		{
			lines.push_back(line);
			totalSize += line.size() + 2;
		}

		stream.close();
	}
}

std::string FileReader::GetString() const
{
	std::string buffer;
	for (const auto& line : lines)
	{
		buffer.append(line);
		buffer.append("\r\n");
	}
	return buffer;
}

std::string FileSystem::ExpandPath(const std::string& base, const std::string& fragment)
{
	// The fragment is an absolute path, don't modify it.
	if (fragment[0] == '/' || FileSystem::StartsWithWindowsDriveLetter(fragment))
		return fragment;

	// The fragment is relative to a home directory, expand that.
	if (!fragment.compare(0, 2, "~/", 2))
	{
		const char* homedir = getenv("HOME");
		if (homedir && *homedir)
			return std::string(homedir) + '/' + fragment.substr(2);
	}

	return base + '/' + fragment;
}

bool FileSystem::FileExists(const std::string& file)
{
	struct stat sb;
	if (stat(file.c_str(), &sb) == -1)
		return false;

	if ((sb.st_mode & S_IFDIR) > 0)
		return false;

	return !access(file.c_str(), F_OK);
}

std::string FileSystem::GetFileName(const std::string& name)
{
#ifdef _WIN32
	size_t pos = name.find_last_of("\\/");
#else
	size_t pos = name.rfind('/');
#endif
	return pos == std::string::npos ? name : name.substr(++pos);
}

bool FileSystem::StartsWithWindowsDriveLetter(const std::string& path)
{
	return (path.length() > 2 && isalpha(path[0]) && path[1] == ':');
}
