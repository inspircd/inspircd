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
	FLAG_USE_XML  = 0x1,
	/** Any include is safe, including executables */
	FLAG_INC_EXEC = 0x00,
	/** Only file-based includes are safe, any path */
	FLAG_INC_FILE = 0x10,
	/** Only relative-path (no /../) includes are safe */
	FLAG_INC_REL  = 0x20,
	/** No includes or <files> allowed */
	FLAG_INC_NONE = 0x30,
	/** Mask */
	FLAG_INC_MASK = 0x30
};

struct ParseStack
{
	std::vector<std::string> reading;
	std::map<std::string, std::string> vars;
	ConfigDataHash& output;
	ConfigFileCache& FilesOutput;
	std::stringstream& errstr;

	ParseStack(ServerConfig* conf)
		: output(conf->config_data), FilesOutput(conf->Files), errstr(conf->status.errors)
	{
		vars["amp"] = "&";
		vars["apos"] = "'";
		vars["quot"] = "\"";
		vars["lt"] = "<";
		vars["gt"] = ">";
		vars["newline"] = vars["nl"] = "\n";
	}
	bool ParseFile(const std::string& name, int flags);
	bool ParseExec(const std::string& name, int flags);
	void DoInclude(ConfigTag* includeTag, int flags);
	void DoReadFile(const std::string& key, const std::string& file, int flags, bool exec);
};

/** RAII wrapper on FILE* to close files on exceptions */
struct FileWrapper
{
	FILE* const f;
	FileWrapper(FILE* file) : f(file) {}
	operator FILE*() const { return f; }
	~FileWrapper()
	{
		if (f)
			fclose(f);
	}
};


