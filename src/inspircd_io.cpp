/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  Inspire is copyright (C) 2002-2003 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *           	  <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 
 $Log$
 Revision 1.1  2003/01/23 19:45:58  brain
 Initial revision

 Revision 1.12  2003/01/22 20:49:16  brain
 Added FileReader file-caching class
 Changed m_randquote to use FileReader class

 Revision 1.11  2003/01/21 20:31:24  brain
 Modified to add documentation
 Added ConfigReader class for modules

 Revision 1.10  2003/01/16 20:11:55  brain
 fixed some ugly pointer bugs (thanks dblack and a|KK|y!)

 Revision 1.9  2003/01/09 13:05:58  brain

 Fixed socket lingering problems (is BSD compatible)

 Revision 1.8  2003/01/07 19:57:56  brain

 Dynamix module support, preliminary release

 Revision 1.7  2003/01/06 23:38:29  brain

 just playing with header tags


 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "inspircd_io.h"
#include "inspircd_util.h"

void WriteOpers(char* text, ...);

void Exit (int status)
{
  send_error("Server shutdown.");
  exit (status);
}

void Killed(int status)
{
  send_error("Server terminated.");
  exit(status);
}

void Rehash(int status)
{
  ReadConfig();
  WriteOpers("Rehashing config file %s due to SIGHUP",CONFIG_FILE);
}



void Start (void)
{
  printf("\033[1;37mInspire Internet Relay Chat Server, compiled " __DATE__ " at " __TIME__ "\n");
  printf("(C) ChatSpike Development team.\033[0;37m\n\n");
  printf("\033[1;37mDevelopers:\033[0;37m     Brain, FrostyCoolSlug, Raider, RD\n");
  printf("\033[1;37mDocumentation:\033[0;37m  FrostyCoolSlug\n");
  printf("\033[1;37mTesters:\033[0;37m        MrBOFH, piggles, Lord_Zathras, typobox43, CC\n");
  printf("\033[1;37mName concept:\033[0;37m   Lord_Zathras\n\n");
}


int DaemonSeed (void)
{
  int childpid;
  signal (SIGALRM, SIG_IGN);
  signal (SIGHUP, Rehash);
  signal (SIGPIPE, SIG_IGN);
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
  umask (077);
  /* close stdout, stdin, stderr */
  close(0);
  close(1);
  close(2);
  return (TRUE);
}




/* Make sure the config file is available */
int CheckConfig (void)
{
  FILE *input;

  if ((input = fopen (CONFIG_FILE, "r")) == NULL)
    {
      printf("ERROR: Cannot open config file: %s\nExiting...\n",CONFIG_FILE);
      return(FALSE);
    }
  else
    fclose (input);

return(TRUE);
}

/* Counts the number of tags of a certain type within the config file, e.g. to enumerate opers */

int EnumConf(const char* filename, const char* tag)
{
	FILE *config;
	int ptr = 0;
	char buffer[MAXBUF], c_tag[MAXBUF], c;
	int in_token, in_quotes, tptr, j, idx = 0;
	char* key;

	if ((config = fopen (filename, "r")) == NULL)
	{
		return 0;
	}
	else
	{
		ptr = 0;
		in_token = 0;
		in_quotes = 0;
		while (!feof(config))
		{
			c = fgetc(config);
			if ((c == '<') && (!in_quotes))
			{
				tptr = 0;
				in_token = 1;
				do {
					c = fgetc(config);
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
	}
	fclose(config);
	return idx;
}



int ConfValueEnum(char* tag)
{
	EnumConf(CONFIG_FILE,tag);
}



/* Retrieves a value from the config file. If there is more than one value of the specified
 * key and section (e.g. for opers etc) then the index value specifies which to retreive, e.g.
 *
 * ConfValue("oper","name",2,result);
 */

int ReadConf(const char* filename, const char* tag, const char* var, int index, char *result)
{
	FILE *config;
	int ptr = 0;
	char buffer[MAXBUF], c_tag[MAXBUF], c;
	int in_token, in_quotes, tptr, j, idx = 0;
	char* key;

	if ((config = fopen (filename, "r")) == NULL)
	{
		return 0;
	}
	else
	{
		ptr = 0;
		in_token = 0;
		in_quotes = 0;
		while (!feof(config))
		{
			c = fgetc(config);
			if ((c == '<') && (!in_quotes))
			{
				tptr = 0;
				in_token = 1;
				do {
					c = fgetc(config);
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
								fclose(config);
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
								fclose(config);
								strcpy(result,key);
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
	}
	fclose(config);
	return 0;
}



int ConfValue(char* tag, char* var, int index, char *result)
{
	ReadConf(CONFIG_FILE, tag, var, index, result);
}



/* This will bind a socket to a port. It works for UDP/TCP */
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


/* Open a TCP Socket */
int OpenTCPSocket (void)
{
  int sockfd;
  int on = 0;
  struct linger linger = { 0 };
  
  if ((sockfd = socket (AF_INET, SOCK_STREAM, 0)) < 0)
    return (ERROR);
  else
  {
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof(on));
    /* This is BSD compatible, setting l_onoff to 0 is *NOT* http://web.irc.org/mla/ircd-dev/msg02259.html */
    linger.l_onoff = 1;
    linger.l_linger = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_LINGER, (const char*)&linger,sizeof(linger));
    return (sockfd);
  }
}

