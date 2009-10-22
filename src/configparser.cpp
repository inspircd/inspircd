/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include <fstream>
#include "configparser.h"

struct Parser
{
	ParseStack& stack;
	const int flags;
	FILE* const file;
	fpos current;
	fpos last_tag;
	reference<ConfigTag> tag;
	int ungot;

	Parser(ParseStack& me, int myflags, FILE* conf, const std::string& name)
		: stack(me), flags(myflags), file(conf), current(name), last_tag(name), ungot(-1)
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
		while (isalnum(ch) || ch == '_')
		{
			rv.push_back(ch);
			ch = next();
		}
		unget(ch);
	}

	bool kv()
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
			if (ch == '&')
			{
				std::string varname;
				while (1)
				{
					ch = next();
					if (isalnum(ch))
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
				std::map<std::string, std::string>::iterator var = stack.vars.find(varname);
				if (var == stack.vars.end())
					throw CoreException("Undefined XML entity reference '&" + varname + ";'");
				value.append(var->second);
			}
			else if (ch == '"')
				break;
			value.push_back(ch);
		}
		tag->items.push_back(KeyVal(key, value));
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

		tag = new ConfigTag(name, current.filename, current.line);

		while (kv());

		if (name == "include")
		{
			stack.DoInclude(tag, flags);
		}
		else if (name == "define")
		{
			std::string varname = tag->getString("name");
			std::string value = tag->getString("value");
			if (varname.empty())
				throw CoreException("Variable definition must include variable name");
			stack.vars[varname] = value;
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
	std::string name;
	if (tag->readString("file", name))
	{
		if (tag->getBool("noinclude", false))
			flags |= FLAG_NO_INC;
		if (tag->getBool("noexec", false))
			flags |= FLAG_NO_EXEC;
		if (!ParseFile(name, flags))
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
		if (!ParseExec(name, flags))
			throw CoreException("Included");
	}
}

bool ParseStack::ParseFile(const std::string& name, int flags)
{
	ServerInstance->Logs->Log("CONFIG", DEBUG, "Reading file %s", name.c_str());
	for (unsigned int t = 0; t < reading.size(); t++)
	{
		if (std::string(name) == reading[t])
		{
			throw CoreException("File " + name + " is included recursively (looped inclusion)");
		}
	}

	/* It's not already included, add it to the list of files we've loaded */

	FileWrapper file(fopen(name.c_str(), "r"));
	if (!file)
		throw CoreException("Could not read \"" + name + "\" for include");

	reading.push_back(name);
	Parser p(*this, flags, file, name);
	bool ok = p.outer_parse();
	reading.pop_back();
	return ok;
}

bool ParseStack::ParseExec(const std::string& name, int flags)
{
	ServerInstance->Logs->Log("CONFIG", DEBUG, "Reading executable %s", name.c_str());
	for (unsigned int t = 0; t < reading.size(); t++)
	{
		if (std::string(name) == reading[t])
		{
			throw CoreException("Executable " + name + " is included recursively (looped inclusion)");
		}
	}

	/* It's not already included, add it to the list of files we've loaded */

	FileWrapper file(popen(name.c_str(), "r"));
	if (!file)
		throw CoreException("Could not open executable \"" + name + "\" for include");

	reading.push_back(name);
	Parser p(*this, flags, file, name);
	bool ok = p.outer_parse();
	reading.pop_back();
	return ok;
}

bool ConfigTag::readString(const std::string& key, std::string& value, bool allow_lf)
{
	if (!this)
		return false;
	for(std::vector<KeyVal>::iterator j = items.begin(); j != items.end(); ++j)
	{
		if(j->first != key)
			continue;
		value = j->second;
 		if (!allow_lf && (value.find('\n') != std::string::npos))
		{
			ServerInstance->Logs->Log("CONFIG",DEFAULT, "Value of <" + tag + ":" + key + "> at " + getTagLocation() +
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

long ConfigTag::getInt(const std::string &key, long def)
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
			res= res* 1024;
			break;
		case 'M':
			res= res* 1024 * 1024;
			break;
		case 'G':
			res= res* 1024 * 1024 * 1024;
			break;
	}
	return res;
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

	return (result == "yes" || result == "true" || result == "1" || result == "on");
}

std::string ConfigTag::getTagLocation()
{
	return src_name + ":" + ConvToStr(src_line);
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
