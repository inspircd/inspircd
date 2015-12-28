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


#include "inspircd.h"

#include <fstream>

#if defined _WIN32
# define PATH_MAX MAX_PATH
#endif

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
		const std::string realName = ServerInstance->Config->Paths.PrependConfig(filename);
		lines.clear();

		std::ifstream stream(realName.c_str());
		if (!stream.is_open())
			throw CoreException(filename + " does not exist or is not readable!");

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
	for (file_cache::const_iterator it = this->lines.begin(); it != this->lines.end(); ++it)
	{
		buffer.append(*it);
		buffer.append("\r\n");
	}
	return buffer;
}

std::string FileSystem::ExpandPath(const std::string& base, const std::string& fragment)
{
	// The fragment is an absolute path, don't modify it.
	if (fragment[0] == '/' || FileSystem::StartsWithWindowsDriveLetter(fragment))
		return fragment;

	// The fragment is a path relative to the starting directory.
	if (fragment.size() >= 2 && fragment[0] == '.' && fragment[1] == '/')
	{
		char path[PATH_MAX + 1];
		if (getcwd(path, sizeof(path)))
		{
			return std::string(path) + '/' + fragment.substr(2);
		}

		// I'm not sure what else we could do here other than log and fall through to the default expansion.
		ServerInstance->Logs->Log("FILESYSTEM", LOG_DEFAULT, "Unable to expand %s: %s", fragment.c_str(), strerror(errno));
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
