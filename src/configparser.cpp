/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
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
	int flags;
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
		while (isalnum(ch) || ch == '_'|| ch == '-')
		{
			rv.push_back(ch);
			ch = next();
		}
		unget(ch);
	}

	bool kv(std::vector<KeyVal>* items, std::set<std::string>& seen)
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
		else if (ch == '/' && key.empty())
		{
			// self-closing tags like <connect allow="*" />
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
			if (ch == '&' && (flags & FLAG_USE_XML))
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
			else if (ch == '\\' && !(flags & FLAG_USE_XML))
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
			else
				value.push_back(ch);
		}

		if (!seen.insert(key).second)
			throw CoreException("Duplicate key '" + key + "' found");

		items->push_back(KeyVal(key, value));
		return true;
	}

	void dotag()
	{
		last_tag = current;
		std::string name;
		nextword(name);

		int spc = next();
		if (spc == '/' && name.empty())
		{
			// XML close tag like "</tag>"; ignore.
			nextword(name);
			spc = next();
			if (spc != '>')
				throw CoreException("Invalid characters in XML closing tag");
			return;
		}

		// Except for <tag> and <tag/>, the name must be followed by a space
		if (spc == '>' || spc == '/')
			unget(spc);
		else if (!isspace(spc))
			throw CoreException("Invalid character in tag name");

		if (name.empty())
			throw CoreException("Empty tag name");

		std::vector<KeyVal>* items;
		std::set<std::string> seen;
		tag = ConfigTag::create(name, current.filename, current.line, items);

		while (kv(items, seen));

		if (name == "include")
		{
			stack.DoInclude(tag, flags);
		}
		else if (name == "files")
		{
			for(std::vector<KeyVal>::iterator i = items->begin(); i != items->end(); i++)
			{
				stack.DoReadFile(i->first, i->second, flags, false);
			}
		}
		else if (name == "execfiles")
		{
			for(std::vector<KeyVal>::iterator i = items->begin(); i != items->end(); i++)
			{
				stack.DoReadFile(i->first, i->second, flags, true);
			}
		}
		else if (name == "define")
		{
			if (!(flags & FLAG_USE_XML))
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
				flags |= FLAG_USE_XML;
			else if (format == "compat")
				flags &= ~FLAG_USE_XML;
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

static int AllowToInc(const std::string& allow)
{
	if (allow == "exec")
		return FLAG_INC_EXEC;
	if (allow == "file")
		return FLAG_INC_FILE;
	if (allow == "relative")
		return FLAG_INC_REL;
	if (allow == "none")
		return FLAG_INC_NONE;
	throw CoreException("Invalid value for <include:allow> - must be one of exec, file, relative, none");
}

void ParseStack::DoInclude(ConfigTag* tag, int flags)
{
	int inc = (flags & FLAG_INC_MASK);
	if (inc == FLAG_INC_NONE)
		throw CoreException("Invalid <include> tag in file included with noinclude=\"yes\"");
	int newinc = inc;
	std::string allow;
	if (tag->readString("allow", allow))
		newinc = AllowToInc(allow);
	else if (tag->getBool("noinclude", false))
		newinc = FLAG_INC_NONE;
	else if (tag->getBool("noexec", false))
		newinc = FLAG_INC_FILE;

	if (newinc < inc)
		throw CoreException("You cannot widen permissions using <include:allow>; adjust the parent <include> block");
	flags = (flags & ~FLAG_INC_MASK) | newinc;

	std::string name;
	if (tag->readString("file", name))
	{
		if (inc == FLAG_INC_REL)
		{
			if (name[0] == '/')
				throw CoreException("You cannot use absolute paths in a file included with <include:allow=\"relative\">");
			if (name.find("/../") != std::string::npos || name.find("../") == 0)
				throw CoreException("You cannot use paths containing \"..\" in a file included with <include:allow=\"relative\">");
		}
		if (!ParseFile(name, flags))
			throw CoreException("Included");
	}
	else if (tag->readString("executable", name))
	{
		if (inc != FLAG_INC_EXEC)
			throw CoreException("Invalid <include:executable> tag in file without executable include permission. Use <include allow=\"exec\"> in the parent file to override this.");
		// by default, don't allow <include:executable> to stack
		if (allow.empty() && tag->getBool("noexec", true))
			flags = (flags & ~FLAG_INC_MASK) | FLAG_INC_FILE;
		if (!ParseExec(name, flags))
			throw CoreException("Included");
	}
}

void ParseStack::DoReadFile(const std::string& key, const std::string& name, int flags, bool exec)
{
	int inc = (flags & FLAG_INC_MASK);
	if (exec && inc != FLAG_INC_EXEC)
		throw CoreException("Invalid <execfiles> tag in file without executable include permission. Use <include allow=\"exec\"> in the parent file to override this.");
	if (inc == FLAG_INC_NONE)
		throw CoreException("Invalid <files> tag in file included without file include permission. Use <include:allow> to change this.");
	if (inc == FLAG_INC_REL)
	{
		if (name[0] == '/')
			throw CoreException("You cannot use absolute paths in a file included with <include:allow=\"relative\">");
		if (name.find("/../") != std::string::npos || name.find("../") == 0)
			throw CoreException("You cannot use paths containing \"..\" in a file included with <include:allow=\"relative\">");
	}

	FileWrapper file(exec ? popen(name.c_str(), "r") : fopen(name.c_str(), "r"));
	if (!file)
		throw CoreException("Could not read \"" + name + "\" for \"" + key + "\" file");

	file_cache& cache = FilesOutput[key];
	cache.clear();

	char linebuf[MAXBUF*10];
	while (fgets(linebuf, sizeof(linebuf), file))
	{
		int len = strlen(linebuf);
		if (len)
			cache.push_back(std::string(linebuf, len - 1));
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

	if (result == "yes" || result == "true" || result == "1" || result == "on")
		return true;
	if (result == "no" || result == "false" || result == "0" || result == "off")
		return false;

	ServerInstance->Logs->Log("CONFIG",DEFAULT, "Value of <" + tag + ":" + key + "> at " + getTagLocation() +
		" is not valid, ignoring");
	return def;
}

std::string ConfigTag::getTagLocation()
{
	return src_name + ":" + ConvToStr(src_line);
}

ConfigTag* ConfigTag::create(const std::string& Tag, const std::string& file, int line, std::vector<KeyVal>*&items)
{
	ConfigTag* rv = new ConfigTag(Tag, file, line);
	items = &rv->items;
	return rv;
}

ConfigTag::ConfigTag(const std::string& Tag, const std::string& file, int line)
	: tag(Tag), src_name(file), src_line(line)
{
}

std::string OperInfo::getConfig(const std::string& key)
{
	std::string rv;
	for(std::vector<reference<ConfigTag> >::iterator i = config_blocks.begin(); i != config_blocks.end(); i++)
	{
		if ((**i).readString(key, rv))
			return rv;
	}
	return rv;
}
