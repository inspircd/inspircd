/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
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

struct fpos
{
	std::string filename;
	int line;
	int col;
	fpos(const std::string& name, int l = 1, int c = 1) : filename(name), line(l), col(c) {}
	std::string str()
	{
		return filename + ":" + ConvToStr(line) + ":" + ConvToStr(col);
	}
};

enum ParseFlags
{
	FLAG_USE_COMPAT = 1,
	FLAG_NO_EXEC = 2,
	FLAG_NO_INC = 4
};

struct ParseStack
{
	std::vector<std::string> reading;
	insp::flat_map<std::string, std::string, irc::insensitive_swo> vars;
	ConfigDataHash& output;
	ConfigFileCache& FilesOutput;
	std::stringstream& errstr;

	ParseStack(ServerConfig* conf)
		: output(conf->config_data), FilesOutput(conf->Files), errstr(conf->errstr)
	{
		vars["amp"] = "&";
		vars["quot"] = "\"";
		vars["newline"] = vars["nl"] = "\n";
	}
	bool ParseFile(const std::string& name, int flags, const std::string& mandatory_tag = std::string(), bool isexec = false);
	void DoInclude(ConfigTag* includeTag, int flags);
	void DoReadFile(const std::string& key, const std::string& file, int flags, bool exec);
};

/** RAII wrapper on FILE* to close files on exceptions */
struct FileWrapper
{
	FILE* const f;
	bool close_with_pclose;
	FileWrapper(FILE* file, bool use_pclose = false) : f(file), close_with_pclose(use_pclose) {}
	operator bool() { return (f != NULL); }
	operator FILE*() { return f; }
	~FileWrapper()
	{
		if (f)
		{
			if (close_with_pclose)
				pclose(f);
			else
				fclose(f);
		}
	}
};
