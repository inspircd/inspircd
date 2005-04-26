/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  Inspire is copyright (C) 2002-2004 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *           	  <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <string>
#include <unistd.h>
#include <sstream>
#include <iostream>
#include <fstream>
#include "inspircd.h"
#include "inspircd_io.h"
#include "inspircd_util.h"
#include "inspstring.h"

using namespace std;

extern FILE *log_file;
extern int boundPortCount;
extern int openSockfd[MAXSOCKS];
extern time_t TIME;
extern bool unlimitcore;

void WriteOpers(char* text, ...);

void Exit (int status)
{
	if (log_file)
		fclose(log_file);
	send_error("Server shutdown.");
	exit (status);
}

void Killed(int status)
{
	if (log_file)
		fclose(log_file);
	send_error("Server terminated.");
	exit(status);
}

void Rehash(int status)
{
	WriteOpers("Rehashing config file %s due to SIGHUP",CONFIG_FILE);
	ReadConfig(false,NULL);
}



void Start (void)
{
	printf("\033[1;32mInspire Internet Relay Chat Server, compiled " __DATE__ " at " __TIME__ "\n");
	printf("(C) ChatSpike Development team.\033[0m\n\n");
	printf("Developers:\033[1;32m     Brain, FrostyCoolSlug\033[0m\n");
	printf("Documentation:\033[1;32m  FrostyCoolSlug, w00t\033[0m\n");
	printf("Testers:\033[1;32m        typobox43, piggles, Lord_Zathras, CC\033[0m\n");
	printf("Name concept:\033[1;32m   Lord_Zathras\033[0m\n\n");
}

void WritePID(std::string filename)
{
	ofstream outfile(filename.c_str());
	if (outfile.is_open())
	{
		outfile << getpid();
		outfile.close();
	}
	else
	{
		printf("Failed to write PID-file '%s', exiting.\n",filename.c_str());
		log(DEFAULT,"Failed to write PID-file '%s', exiting.",filename.c_str());
		Exit(0);
	}
}

void DeadPipe(int status)
{
  signal (SIGPIPE, DeadPipe);
}

int DaemonSeed (void)
{
	int childpid;
	signal (SIGALRM, SIG_IGN);
	signal (SIGHUP, Rehash);
	signal (SIGPIPE, DeadPipe);
	signal (SIGTERM, Exit);
	signal (SIGABRT, Exit);
	signal (SIGSEGV, Error);
	signal (SIGURG, Exit);
	signal (SIGKILL, Exit);
	if ((childpid = fork ()) < 0)
		return (ERROR);
	else if (childpid > 0)
		exit (0);
	setsid ();
	umask (007);
	printf("InspIRCd Process ID: \033[1;32m%lu\033[0m\n",(unsigned long)getpid());
	freopen("/dev/null","w",stdout);
	freopen("/dev/null","w",stderr);
	
	setpriority(PRIO_PROCESS,(int)getpid(),15);

	if (unlimitcore)
	{
		rlimit rl;
		if (getrlimit(RLIMIT_CORE, &rl) == -1)
		{
			log(DEFAULT,"Failed to getrlimit()!");
			return(FALSE);
		}
		else
		{
			rl.rlim_cur = rl.rlim_max;
			if (setrlimit(RLIMIT_CORE, &rl) == -1)
				log(DEFAULT,"setrlimit() failed, cannot increase coredump size.");
		}
	}
  
	return (TRUE);
}


/* Make Sure Modules Are Avaliable!
 * (BugFix By Craig.. See? I do work! :p)
 * Modified by brain, requires const char*
 * to work with other API functions
 */

bool FileExists (const char* file)
{
	FILE *input;
	if ((input = fopen (file, "r")) == NULL)
	{
		return(false);
	}
	else
	{
		fclose (input);
		return(true);
	}
}

/* ConfProcess does the following things to a config line in the following order:
 *
 * Processes the line for syntax errors as shown below
 *      (1) Line void of quotes or equals (a malformed, illegal tag format)
 *      (2) Odd number of quotes on the line indicating a missing quote
 *      (3) number of equals signs not equal to number of quotes / 2 (missing an equals sign)
 *      (4) Spaces between the opening bracket (<) and the keyword
 *      (5) Spaces between a keyword and an equals sign
 *      (6) Spaces between an equals sign and a quote
 * Removes trailing spaces
 * Removes leading spaces
 * Converts tabs to spaces
 * Turns multiple spaces that are outside of quotes into single spaces
 */

std::string ConfProcess(char* buffer, long linenumber, std::stringstream* errorstream, bool &error, std::string filename)
{
	long number_of_quotes = 0;
	long number_of_equals = 0;
	bool has_open_bracket = false;
	bool in_quotes = false;
	error = false;
	if (!buffer)
	{
		return "";
	}
	// firstly clean up the line by stripping spaces from the start and end and converting tabs to spaces
	for (int d = 0; d < strlen(buffer); d++)
		if ((buffer[d]) == 9)
			buffer[d] = ' ';
	while ((buffer[0] == ' ') && (strlen(buffer)>0)) buffer++;
	while ((buffer[strlen(buffer)-1] == ' ') && (strlen(buffer)>0)) buffer[strlen(buffer)-1] = '\0';
	// empty lines are syntactically valid
	if (!strcmp(buffer,""))
		return "";
	else if (buffer[0] == '#')
		return "";
	for (int c = 0; c < strlen(buffer); c++)
	{
		// convert all spaces that are OUTSIDE quotes into hardspace (0xA0) as this will make them easier to
		// search and replace later :)
		if ((!in_quotes) && (buffer[c] == ' '))
			buffer[c] = '\xA0';
		if ((buffer[c] == '<') && (!in_quotes))
		{
			has_open_bracket = true;
			if (strlen(buffer) == 1)
			{
				*errorstream << "Tag without identifier at " << filename << ":" << linenumber << endl;
				error = true;
				return "";
			}
			else if ((tolower(buffer[c+1]) < 'a') || (tolower(buffer[c+1]) > 'z'))
			{
				*errorstream << "Invalid characters in identifier at " << filename << ":" << linenumber << endl;
				error = true;
				return "";
			}
		}
		if (buffer[c] == '"')
		{
			number_of_quotes++;
			in_quotes = (!in_quotes);
		}
		if ((buffer[c] == '=') && (!in_quotes))
		{
			number_of_equals++;
			if (strlen(buffer) == c)
			{
				*errorstream << "Variable without a value at " << filename << ":" << linenumber << endl;
				error = true;
				return "";
			}
			else if (buffer[c+1] != '"')
			{
				*errorstream << "Variable name not followed immediately by its value at " << filename << ":" << linenumber << endl;
				error = true;
				return "";
			}
			else if (!c)
			{
				*errorstream << "Value without a variable (line starts with '=') at " << filename << ":" << linenumber << endl;
				error = true;
				return "";
			}
			else if (buffer[c-1] == '\xA0')
			{
				*errorstream << "Variable name not followed immediately by its value at " << filename << ":" << linenumber << endl;
				error = true;
				return "";
			}
		}
	}
	// no quotes, and no equals. something freaky.
	if ((!number_of_quotes) || (!number_of_equals) && (strlen(buffer)>2) && (buffer[0]=='<'))
	{
		*errorstream << "Malformed tag at " << filename << ":" << linenumber << endl;
		error = true;
		return "";
	}
	// odd number of quotes. thats just wrong.
	if ((number_of_quotes % 2) != 0)
	{
		*errorstream << "Missing \" at " << filename << ":" << linenumber << endl;
		error = true;
		return "";
	}
	if (number_of_equals < (number_of_quotes/2))
	{
		*errorstream << "Missing '=' at " << filename << ":" << linenumber << endl;
	}
	if (number_of_equals > (number_of_quotes/2))
	{
		*errorstream << "Too many '=' at " << filename << ":" << linenumber << endl;
	}

	std::string parsedata = buffer;
	// turn multispace into single space
	while (parsedata.find("\xA0\xA0") != std::string::npos)
	{
		parsedata.erase(parsedata.find("\xA0\xA0"),1);
	}

	// turn our hardspace back into softspace
	for (int d = 0; d < parsedata.length(); d++)
	{
		if (parsedata[d] == '\xA0')
			parsedata[d] = ' ';
	}

	// and we're done, the line is fine!
	return parsedata;
}

bool LoadConf(const char* filename, std::stringstream *target, std::stringstream* errorstream)
{
	target->str("");
	errorstream->str("");
	long linenumber = 1;
	// first, check that the file exists before we try to do anything with it
	if (!FileExists(filename))
	{
		*errorstream << "File " << filename << " not found." << endl;
		return false;
	}
	// Fix the chmod of the file to restrict it to the current user and group
	chmod(filename,0600);
	// now open it
	FILE* conf = fopen(filename,"r");
	char buffer[MAXBUF];
	if (conf)
	{
		while (!feof(conf))
		{
			if (fgets(buffer, MAXBUF, conf))
			{
				if ((!feof(conf)) && (buffer) && (strlen(buffer)))
				{
					if ((buffer[0] != '#') && (buffer[0] != '\r')  && (buffer[0] != '\n'))
					{
						bool error = false;
						std::string data = ConfProcess(buffer,linenumber++,errorstream,error,filename);
						if (error)
						{
							return false;
						}
						*target << data;
					}
					else linenumber++;
				}
			}
		}
		fclose(conf);
	}
	target->seekg(0);
	return true;
}

/* Counts the number of tags of a certain type within the config file, e.g. to enumerate opers */

int EnumConf(std::stringstream *config, const char* tag)
{
	int ptr = 0;
	char buffer[MAXBUF], c_tag[MAXBUF], c, lastc;
	int in_token, in_quotes, tptr, idx = 0;

	const char* buf = config->str().c_str();
	long bptr = 0;
	long len = strlen(buf);
	
	ptr = 0;
	in_token = 0;
	in_quotes = 0;
	lastc = '\0';
	while (bptr<len)
	{
		lastc = c;
		c = buf[bptr++];
		if ((c == '#') && (lastc == '\n'))
		{
			while ((c != '\n') && (bptr<len))
			{
				lastc = c;
				c = buf[bptr++];
			}
		}
		if ((c == '<') && (!in_quotes))
		{
			tptr = 0;
			in_token = 1;
			do {
				c = buf[bptr++];
				if (c != ' ')
				{
					c_tag[tptr++] = c;
					c_tag[tptr] = '\0';
				}
			} while (c != ' ');
		}
		if (c == '"')
		{
			in_quotes = (!in_quotes);
		}
		if ((c == '>') && (!in_quotes))
		{
			in_token = 0;
			if (!strcmp(c_tag,tag))
			{
				/* correct tag, but wrong index */
				idx++;
			}
			c_tag[0] = '\0';
			buffer[0] = '\0';
			ptr = 0;
			tptr = 0;
		}
		if (c != '>')
		{
			if ((in_token) && (c != '\n') && (c != '\r'))
			{
				buffer[ptr++] = c;
				buffer[ptr] = '\0';
			}
		}
	}
	return idx;
}

/* Counts the number of values within a certain tag */

int EnumValues(std::stringstream *config, const char* tag, int index)
{
	int ptr = 0;
	char buffer[MAXBUF], c_tag[MAXBUF], c, lastc;
	int in_token, in_quotes, tptr, idx = 0;
	
	bool correct_tag = false;
	int num_items = 0;

	const char* buf = config->str().c_str();
	long bptr = 0;
	long len = strlen(buf);
	
	ptr = 0;
	in_token = 0;
	in_quotes = 0;
	lastc = '\0';
	while (bptr<len)
	{
		lastc = c;
		c = buf[bptr++];
		if ((c == '#') && (lastc == '\n'))
		{
			while ((c != '\n') && (bptr<len))
			{
				lastc = c;
				c = buf[bptr++];
			}
		}
		if ((c == '<') && (!in_quotes))
		{
			tptr = 0;
			in_token = 1;
			do {
				c = buf[bptr++];
				if (c != ' ')
				{
					c_tag[tptr++] = c;
					c_tag[tptr] = '\0';
					
					if ((!strcmp(c_tag,tag)) && (idx == index))
					{
						correct_tag = true;
					}
				}
			} while (c != ' ');
		}
		if (c == '"')
		{
			in_quotes = (!in_quotes);
		}
		
		if ( (correct_tag) && (!in_quotes) && ( (c == ' ') || (c == '\n') || (c == '\r') ) )
		{
			num_items++;
		}
		if ((c == '>') && (!in_quotes))
		{
			in_token = 0;
			if (correct_tag)
				correct_tag = false;
			if (!strcmp(c_tag,tag))
			{
				/* correct tag, but wrong index */
				idx++;
			}
			c_tag[0] = '\0';
			buffer[0] = '\0';
			ptr = 0;
			tptr = 0;
		}
		if (c != '>')
		{
			if ((in_token) && (c != '\n') && (c != '\r'))
			{
				buffer[ptr++] = c;
				buffer[ptr] = '\0';
			}
		}
	}
	return num_items+1;
}



int ConfValueEnum(char* tag, std::stringstream* config)
{
	return EnumConf(config,tag);
}



/* Retrieves a value from the config file. If there is more than one value of the specified
 * key and section (e.g. for opers etc) then the index value specifies which to retreive, e.g.
 *
 * ConfValue("oper","name",2,result);
 */

int ReadConf(std::stringstream *config, const char* tag, const char* var, int index, char *result)
{
	int ptr = 0;
	char buffer[65535], c_tag[MAXBUF], c, lastc;
	int in_token, in_quotes, tptr, j, idx = 0;
	char* key;

	const char* buf = config->str().c_str();
	long bptr = 0;
	long len = strlen(buf);
	
	ptr = 0;
	in_token = 0;
	in_quotes = 0;
	lastc = '\0';
	c_tag[0] = '\0';
	buffer[0] = '\0';
	while (bptr<len)
	{
		lastc = c;
		c = buf[bptr++];
		// FIX: Treat tabs as spaces
		if (c == 9)
			c = 32;
		if ((c == '<') && (!in_quotes))
		{
			tptr = 0;
			in_token = 1;
			do {
				c = buf[bptr++];
				if (c != ' ')
				{
					c_tag[tptr++] = c;
					c_tag[tptr] = '\0';
				}
			// FIX: Tab can follow a tagname as well as space.
			} while ((c != ' ') && (c != 9));
		}
		if (c == '"')
		{
			in_quotes = (!in_quotes);
		}
		if ((c == '>') && (!in_quotes))
		{
			in_token = 0;
			if (idx == index)
			{
				if (!strcmp(c_tag,tag))
				{
					if ((buffer) && (c_tag) && (var))
					{
						key = strstr(buffer,var);
						if (!key)
						{
							/* value not found in tag */
							strcpy(result,"");
							return 0;
						}
						else
						{
							key+=strlen(var);
							while (key[0] !='"')
							{
								if (!strlen(key))
								{
									/* missing quote */
									strcpy(result,"");
									return 0;
								}
								key++;
							}
							key++;
							for (j = 0; j < strlen(key); j++)
							{
								if (key[j] == '"')
								{
									key[j] = '\0';
								}
							}
							strlcpy(result,key,MAXBUF);
							return 1;
						}
					}
				}
			}
			if (!strcmp(c_tag,tag))
			{
				/* correct tag, but wrong index */
				idx++;
			}
			c_tag[0] = '\0';
			buffer[0] = '\0';
			ptr = 0;
			tptr = 0;
		}
		if (c != '>')
		{
			if ((in_token) && (c != '\n') && (c != '\r'))
			{
				buffer[ptr++] = c;
				buffer[ptr] = '\0';
			}
		}
	}
	strcpy(result,""); // value or its tag not found at all
	return 0;
}



int ConfValue(char* tag, char* var, int index, char *result,std::stringstream *config)
{
	ReadConf(config, tag, var, index, result);
	return 0;
}



// This will bind a socket to a port. It works for UDP/TCP
int BindSocket (int sockfd, struct sockaddr_in client, struct sockaddr_in server, int port, char* addr)
{
	bzero((char *)&server,sizeof(server));
	struct in_addr addy;
	inet_aton(addr,&addy);
	server.sin_family = AF_INET;
	if (!strcmp(addr,""))
	{
		server.sin_addr.s_addr = htonl(INADDR_ANY);
	}
	else
	{
		server.sin_addr = addy;
	}
	server.sin_port = htons(port);
	if (bind(sockfd,(struct sockaddr*)&server,sizeof(server))<0)
	{
		return(ERROR);
	}
	else
	{
		listen(sockfd,5);
		return(TRUE);
	}
}


// Open a TCP Socket
int OpenTCPSocket (void)
{
	int sockfd;
	int on = 1;
	struct linger linger = { 0 };
  
	if ((sockfd = socket (AF_INET, SOCK_STREAM, 0)) < 0)
		return (ERROR);
	else
	{
		setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof(on));
		/* This is BSD compatible, setting l_onoff to 0 is *NOT* http://web.irc.org/mla/ircd-dev/msg02259.html */
		linger.l_onoff = 1;
		linger.l_linger = 1;
		setsockopt(sockfd, SOL_SOCKET, SO_LINGER, (const char*)&linger,sizeof(linger));
		return (sockfd);
	}
}

