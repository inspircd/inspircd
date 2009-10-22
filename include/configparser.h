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
	FLAG_NO_EXEC = 1,
	FLAG_NO_INC = 2
};

struct ParseStack
{
	std::vector<std::string> reading;
	std::map<std::string, std::string> vars;
	ConfigDataHash& output;
	std::stringstream& errstr;

	ParseStack(ServerConfig* conf)
		: output(conf->config_data), errstr(conf->errstr)
	{
		vars["amp"] = "&";
		vars["quot"] = "\"";
		vars["newline"] = vars["nl"] = "\n";
	}
	bool ParseFile(const std::string& name, int flags);
	bool ParseExec(const std::string& name, int flags);
	void DoInclude(ConfigTag* includeTag, int flags);
};

/** RAII wrapper on FILE* to close files on exceptions */
struct FileWrapper
{
	FILE* const f;
	FileWrapper(FILE* file) : f(file) {}
	operator bool() { return f; }
	operator FILE*() { return f; }
	~FileWrapper()
	{
		if (f)
			fclose(f);
	}
};


