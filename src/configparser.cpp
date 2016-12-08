/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
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
#include "configparser.h"

struct Parser
{
	ParseStack& stack;
	int flags;
	FILE* const file;
	fpos current;
	fpos last_tag;
	reference<ConfigTag> tag;
	int ungot;
	std::string mandatory_tag;

	Parser(ParseStack& me, int myflags, FILE* conf, const std::string& name, const std::string& mandatorytag)
		: stack(me), flags(myflags), file(conf), current(name), last_tag(name), ungot(-1), mandatory_tag(mandatorytag)
	{ }

	int next(bool eof_ok = false)
	{
		if (ungot != -1)
		{
			int ch = ungot;
			ungot = -1;
			return ch;
		}
		int ch = fgetc(file);
		if (ch == EOF && !eof_ok)
		{
			throw CoreException("Unexpected end-of-file");
		}
		else if (ch == '\n')
		{
			current.line++;
			current.col = 0;
		}
		else
		{
			current.col++;
		}
		return ch;
	}

	void unget(int ch)
	{
		if (ungot != -1)
			throw CoreException("INTERNAL ERROR: cannot unget twice");
		ungot = ch;
	}

	void comment()
	{
		while (1)
		{
			int ch = next();
			if (ch == '\n')
				return;
		}
	}

	void nextword(std::string& rv)
	{
		int ch = next();
		while (isspace(ch))
			ch = next();
		while (isalnum(ch) || ch == '_'|| ch == '-')
		{
			rv.push_back(ch);
			ch = next();
		}
		unget(ch);
	}

	bool kv(ConfigItems* items)
	{
		std::string key;
		nextword(key);
		int ch = next();
		if (ch == '>' && key.empty())
		{
			return false;
		}
		else if (ch == '#' && key.empty())
		{
			comment();
			return true;
		}
		else if (ch != '=')
		{
			throw CoreException("Invalid character " + std::string(1, ch) + " in key (" + key + ")");
		}

		std::string value;
		ch = next();
		if (ch != '"')
		{
			throw CoreException("Invalid character in value of <" + tag->tag + ":" + key + ">");
		}
		while (1)
		{
			ch = next();
			if (ch == '&' && !(flags & FLAG_USE_COMPAT))
			{
				std::string varname;
				while (1)
				{
					ch = next();
					if (isalnum(ch) || (varname.empty() && ch == '#'))
						varname.push_back(ch);
					else if (ch == ';')
						break;
					else
					{
						stack.errstr << "Invalid XML entity name in value of <" + tag->tag + ":" + key + ">\n"
							<< "To include an ampersand or quote, use &amp; or &quot;\n";
						throw CoreException("Parse error");
					}
				}
				if (varname.empty())
					throw CoreException("Empty XML entity reference");
				else if (varname[0] == '#' && (varname.size() == 1 || (varname.size() == 2 && varname[1] == 'x')))
					throw CoreException("Empty numeric character reference");
				else if (varname[0] == '#')
				{
					const char* cvarname = varname.c_str();
					char* endptr;
					unsigned long lvalue;
					if (cvarname[1] == 'x')
						lvalue = strtoul(cvarname + 2, &endptr, 16);
					else
						lvalue = strtoul(cvarname + 1, &endptr, 10);
					if (*endptr != '\0' || lvalue > 255)
						throw CoreException("Invalid numeric character reference '&" + varname + ";'");
					value.push_back(static_cast<char>(lvalue));
				}
				else
				{
					insp::flat_map<std::string, std::string>::iterator var = stack.vars.find(varname);
					if (var == stack.vars.end())
						throw CoreException("Undefined XML entity reference '&" + varname + ";'");
					value.append(var->second);
				}
			}
			else if (ch == '\\' && (flags & FLAG_USE_COMPAT))
			{
				int esc = next();
				if (esc == 'n')
					value.push_back('\n');
				else if (isalpha(esc))
					throw CoreException("Unknown escape character \\" + std::string(1, esc));
				else
					value.push_back(esc);
			}
			else if (ch == '"')
				break;
			else if (ch != '\r')
				value.push_back(ch);
		}

		if (items->find(key) != items->end())
			throw CoreException("Duplicate key '" + key + "' found");

		(*items)[key] = value;
		return true;
	}

	void dotag()
	{
		last_tag = current;
		std::string name;
		nextword(name);

		int spc = next();
		if (spc == '>')
			unget(spc);
		else if (!isspace(spc))
			throw CoreException("Invalid character in tag name");

		if (name.empty())
			throw CoreException("Empty tag name");

		ConfigItems* items;
		tag = ConfigTag::create(name, current.filename, current.line, items);

		while (kv(items))
		{
			// Do nothing here (silences a GCC warning).
		}

		if (name == mandatory_tag)
		{
			// Found the mandatory tag
			mandatory_tag.clear();
		}

		if (name == "include")
		{
			stack.DoInclude(tag, flags);
		}
		else if (name == "files")
		{
			for(ConfigItems::iterator i = items->begin(); i != items->end(); i++)
			{
				stack.DoReadFile(i->first, i->second, flags, false);
			}
		}
		else if (name == "execfiles")
		{
			for(ConfigItems::iterator i = items->begin(); i != items->end(); i++)
			{
				stack.DoReadFile(i->first, i->second, flags, true);
			}
		}
		else if (name == "define")
		{
			if (flags & FLAG_USE_COMPAT)
				throw CoreException("<define> tags may only be used in XML-style config (add <config format=\"xml\">)");
			std::string varname = tag->getString("name");
			std::string value = tag->getString("value");
			if (varname.empty())
				throw CoreException("Variable definition must include variable name");
			stack.vars[varname] = value;
		}
		else if (name == "config")
		{
			std::string format = tag->getString("format");
			if (format == "xml")
				flags &= ~FLAG_USE_COMPAT;
			else if (format == "compat")
				flags |= FLAG_USE_COMPAT;
			else if (!format.empty())
				throw CoreException("Unknown configuration format " + format);
		}
		else
		{
			stack.output.insert(std::make_pair(name, tag));
		}
		// this is not a leak; reference<> takes care of the delete
		tag = NULL;
	}

	bool outer_parse()
	{
		try
		{
			while (1)
			{
				int ch = next(true);
				switch (ch)
				{
					case EOF:
						// this is the one place where an EOF is not an error
						if (!mandatory_tag.empty())
							throw CoreException("Mandatory tag \"" + mandatory_tag + "\" not found");
						return true;
					case '#':
						comment();
						break;
					case '<':
						dotag();
						break;
					case ' ':
					case '\r':
					case '\t':
					case '\n':
						break;
					case 0xFE:
					case 0xFF:
						stack.errstr << "Do not save your files as UTF-16; use ASCII!\n";
					default:
						throw CoreException("Syntax error - start of tag expected");
				}
			}
		}
		catch (CoreException& err)
		{
			stack.errstr << err.GetReason() << " at " << current.str();
			if (tag)
				stack.errstr << " (inside tag " << tag->tag << " at line " << tag->src_line << ")\n";
			else
				stack.errstr << " (last tag was on line " << last_tag.line << ")\n";
		}
		return false;
	}
};

void ParseStack::DoInclude(ConfigTag* tag, int flags)
{
	if (flags & FLAG_NO_INC)
		throw CoreException("Invalid <include> tag in file included with noinclude=\"yes\"");

	std::string mandatorytag;
	tag->readString("mandatorytag", mandatorytag);

	std::string name;
	if (tag->readString("file", name))
	{
		if (tag->getBool("noinclude", false))
			flags |= FLAG_NO_INC;
		if (tag->getBool("noexec", false))
			flags |= FLAG_NO_EXEC;
		if (!ParseFile(ServerInstance->Config->Paths.PrependConfig(name), flags, mandatorytag))
			throw CoreException("Included");
	}
	else if (tag->readString("executable", name))
	{
		if (flags & FLAG_NO_EXEC)
			throw CoreException("Invalid <include:executable> tag in file included with noexec=\"yes\"");
		if (tag->getBool("noinclude", false))
			flags |= FLAG_NO_INC;
		if (tag->getBool("noexec", true))
			flags |= FLAG_NO_EXEC;
		if (!ParseFile(name, flags, mandatorytag, true))
			throw CoreException("Included");
	}
}

void ParseStack::DoReadFile(const std::string& key, const std::string& name, int flags, bool exec)
{
	if (flags & FLAG_NO_INC)
		throw CoreException("Invalid <files> tag in file included with noinclude=\"yes\"");
	if (exec && (flags & FLAG_NO_EXEC))
		throw CoreException("Invalid <execfiles> tag in file included with noexec=\"yes\"");

	std::string path = ServerInstance->Config->Paths.PrependConfig(name);
	FileWrapper file(exec ? popen(name.c_str(), "r") : fopen(path.c_str(), "r"), exec);
	if (!file)
		throw CoreException("Could not read \"" + path + "\" for \"" + key + "\" file");

	file_cache& cache = FilesOutput[key];
	cache.clear();

	char linebuf[5120];
	while (fgets(linebuf, sizeof(linebuf), file))
	{
		size_t len = strlen(linebuf);
		if (len)
		{
			if (linebuf[len-1] == '\n')
				len--;
			cache.push_back(std::string(linebuf, len));
		}
	}
}

bool ParseStack::ParseFile(const std::string& path, int flags, const std::string& mandatory_tag, bool isexec)
{
	ServerInstance->Logs->Log("CONFIG", LOG_DEBUG, "Reading (isexec=%d) %s", isexec, path.c_str());
	if (stdalgo::isin(reading, path))
		throw CoreException((isexec ? "Executable " : "File ") + path + " is included recursively (looped inclusion)");

	/* It's not already included, add it to the list of files we've loaded */

	FileWrapper file((isexec ? popen(path.c_str(), "r") : fopen(path.c_str(), "r")), isexec);
	if (!file)
		throw CoreException("Could not read \"" + path + "\" for include");

	reading.push_back(path);
	Parser p(*this, flags, file, path, mandatory_tag);
	bool ok = p.outer_parse();
	reading.pop_back();
	return ok;
}

bool ConfigTag::readString(const std::string& key, std::string& value, bool allow_lf)
{
	for(ConfigItems::iterator j = items.begin(); j != items.end(); ++j)
	{
		if(j->first != key)
			continue;
		value = j->second;
 		if (!allow_lf && (value.find('\n') != std::string::npos))
		{
			ServerInstance->Logs->Log("CONFIG", LOG_DEFAULT, "Value of <" + tag + ":" + key + "> at " + getTagLocation() +
				" contains a linefeed, and linefeeds in this value are not permitted -- stripped to spaces.");
			for (std::string::iterator n = value.begin(); n != value.end(); n++)
				if (*n == '\n')
					*n = ' ';
		}
		return true;
	}
	return false;
}

std::string ConfigTag::getString(const std::string& key, const std::string& def)
{
	std::string res = def;
	readString(key, res);
	return res;
}

long ConfigTag::getInt(const std::string &key, long def, long min, long max)
{
	std::string result;
	if(!readString(key, result))
		return def;

	const char* res_cstr = result.c_str();
	char* res_tail = NULL;
	long res = strtol(res_cstr, &res_tail, 0);
	if (res_tail == res_cstr)
		return def;
	switch (toupper(*res_tail))
	{
		case 'K':
			res = res * 1024;
			break;
		case 'M':
			res = res * 1024 * 1024;
			break;
		case 'G':
			res = res * 1024 * 1024 * 1024;
			break;
	}

	CheckRange(key, res, def, min, max);
	return res;
}

void ConfigTag::CheckRange(const std::string& key, long& res, long def, long min, long max)
{
	if (res < min || res > max)
	{
		ServerInstance->Logs->Log("CONFIG", LOG_DEFAULT, "WARNING: <%s:%s> value of %ld is not between %ld and %ld; set to %ld.",
			tag.c_str(), key.c_str(), res, min, max, def);
		res = def;
	}
}

long ConfigTag::getDuration(const std::string& key, long def, long min, long max)
{
	std::string duration;
	if (!readString(key, duration))
		return def;

	long ret = InspIRCd::Duration(duration);
	CheckRange(key, ret, def, min, max);
	return ret;
}

double ConfigTag::getFloat(const std::string &key, double def)
{
	std::string result;
	if (!readString(key, result))
		return def;
	return strtod(result.c_str(), NULL);
}

bool ConfigTag::getBool(const std::string &key, bool def)
{
	std::string result;
	if(!readString(key, result))
		return def;

	if (result == "yes" || result == "true" || result == "1" || result == "on")
		return true;
	if (result == "no" || result == "false" || result == "0" || result == "off")
		return false;

	ServerInstance->Logs->Log("CONFIG", LOG_DEFAULT, "Value of <" + tag + ":" + key + "> at " + getTagLocation() +
		" is not valid, ignoring");
	return def;
}

std::string ConfigTag::getTagLocation()
{
	return src_name + ":" + ConvToStr(src_line);
}

ConfigTag* ConfigTag::create(const std::string& Tag, const std::string& file, int line, ConfigItems*& Items)
{
	ConfigTag* rv = new ConfigTag(Tag, file, line);
	Items = &rv->items;
	return rv;
}

ConfigTag::ConfigTag(const std::string& Tag, const std::string& file, int line)
	: tag(Tag), src_name(file), src_line(line)
{
}

std::string OperInfo::getConfig(const std::string& key)
{
	std::string rv;
	if (type_block)
		type_block->readString(key, rv);
	if (oper_block)
		oper_block->readString(key, rv);
	return rv;
}
