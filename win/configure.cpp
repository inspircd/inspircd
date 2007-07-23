/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#define _CRT_SECURE_NO_DEPRECATE

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string>
#include <time.h>
#include "inspircd_win32wrapper.h"
#include "colours.h"

using namespace std;
void Run();
void Banner();
void WriteCompileModules();
void WriteCompileCommands();

/* detects if we are running windows xp or higher (5.1) */
bool iswinxp()
{
	OSVERSIONINFO vi;
	vi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	GetVersionEx(&vi);
	if(vi.dwMajorVersion >= 5)
		return true;
	
	return false;
}

int get_int_option(const char * text, int def)
{
	static char buffer[500];
	int ret;
	printf_c("%s\n[\033[1;32m%u\033[0m] -> ", text, def);
	fgets(buffer, 500, stdin);
	if(sscanf(buffer, "%u", &ret) != 1)
		ret = def;

	printf("\n");
	return ret;
}

bool get_bool_option(const char * text, bool def)
{
	static char buffer[500];
	char ret[100];
	printf_c("%s [\033[1;32m%c\033[0m] -> ", text, def ? 'y' : 'n');
	fgets(buffer, 500, stdin);
	if(sscanf(buffer, "%s", ret) != 1)
		strcpy(ret, def ? "y" : "n");

	printf("\n");
	return !strncmp(ret, "y", 1);
}

void get_string_option(const char * text, char * def, char * buf)
{
	static char buffer[500];
	printf_c("%s\n[\033[1;32m%s\033[0m] -> ", text, def);
	fgets(buffer, 500, stdin);
	if(sscanf(buffer, "%s", buf) != 1)
		strcpy(buf, def);

	printf("\n");
}

// escapes a string for use in a c++ file
bool escape_string(char * str, size_t size)
{
	size_t len = strlen(str);
	char * d_str = (char*)malloc(len * 2);
    
	size_t i = 0;
	size_t j = 0;

	for(; i < len; ++i)
	{
		if(str[i] == '\\')
		{
			d_str[j++] = '\\';
			d_str[j++] = '\\';
		}
		else
		{
			d_str[j++] = str[i];
		}
	}

	d_str[j++] = 0;

    if(j > size)
	{
		free(d_str);
		return false;
	}

	strcpy(str, d_str);
	free(d_str);
	return true;
}

/* gets the svn revision */
int get_svn_revision(char * buffer, size_t len)
{
	/* again.. I am lazy :p cbf to pipe output of svn info to us, so i'll just read the file */
	/*
	8

	dir
	7033
	*/
	char buf[1000];
	FILE * f = fopen("..\\.svn\\entries", "r");
	if(!f) goto bad_rev;
    
	if(!fgets(buf, 1000, f)) goto bad_rev;
	if(!fgets(buf, 1000, f)) goto bad_rev;
	if(!fgets(buf, 1000, f)) goto bad_rev;
	if(!fgets(buf, 1000, f)) goto bad_rev;
	int rev = atoi(buf);
	if(rev == 0) goto bad_rev;
	sprintf(buffer, "%u", rev);
	fclose(f);
	return rev;
	
bad_rev:
	strcpy(buffer, "non-svn");
	if(f) fclose(f);
	return 0;
}

int __stdcall WinMain(IN HINSTANCE hInstance, IN HINSTANCE hPrevInstance, IN LPSTR lpCmdLine, IN int nShowCmd )
{
	FILE * j = fopen("inspircd_config.h", "r");
	if (j)
	{
		if (MessageBox(0, "inspircd_config.h already exists. Remove it and build from clean?", "Configure program", MB_ICONQUESTION|MB_YESNO|MB_DEFBUTTON2) != IDYES)
		{
			fclose(j);
			exit(0);
		}
	}

	AllocConsole();

	// pipe standard handles to this console
	freopen("CONIN$", "r", stdin);
	freopen("CONOUT$", "w", stdout);
	freopen("CONOUT$", "w", stderr);

	Banner();
	Run();
	WriteCompileCommands();
	WriteCompileModules();
	FreeConsole();
	return 0;
}

void Banner()
{
	printf_c("\nWelcome to the \033[1mInspIRCd\033[0m Configuration program! (\033[1minteractive mode\033[0m)\n"
			 "\033[1mPackage maintainers: Type ./configure --help for non-interactive help\033[0m\n\n");
	printf_c("*** If you are unsure of any of these values, leave it blank for    ***\n"
			 "*** standard settings that will work, and your server will run      ***\n"
			 "*** using them. Please consult your IRC network admin if in doubt.  ***\n\n"
			 "Press \033[1m<RETURN>\033[0m to accept the default for any option, or enter\n"
			 "a new value. Please note: You will \033[1mHAVE\033[0m to read the docs\n"
			 "dir, otherwise you won't have a config file!\n\n");

}

void Run()
{
	int max_fd = 1024;
	bool use_iocp = false;
	bool support_ip6links = false;
	char mod_path[MAX_PATH];
	char config_file[MAX_PATH];
	char library_dir[MAX_PATH];
	char base_path[MAX_PATH];
	char bin_dir[MAX_PATH];
	char revision_text[MAX_PATH];

	int max_clients = 1024;
	int nicklen = 31;
	int chanlen = 64;
	int modechanges = 20;
	int identlen = 12;
	int quitlen = 255;
	int topiclen = 500;
	int kicklen = 255;
	int rllen = 128;
	int awaylen = 200;
	int revision = get_svn_revision(revision_text, MAX_PATH);
	char version[514];

	// grab version
	FILE * fI = fopen("..\\src\\version.sh", "r");
	if(fI)
	{
		fgets(version, 514, fI);
		fgets(version, 514, fI);
		char * p2 = version;
		while(*p2 != '\"')
			++p2;
		++p2;
		strcpy(version, p2);
		p2 = version;
		while(*p2 != '\"')
			++p2;
		*p2 = 0;
		fclose(fI);
	}
	else
		strcpy(version, "InspIRCD-Unknown");
#ifdef WIN64
	printf_c("Your operating system is: \033[1;32mwindows_x64 \033[0m\n");
#else
	printf_c("Your operating system is: \033[1;32mwindows_x32 \033[0m\n");
#endif
	printf_c("InspIRCd revision ID: \033[1;32m%s \033[0m\n\n", revision ? revision_text : "(Non-SVN build)");

	max_fd = get_int_option("What is the maximum file descriptor count you would like to allow?", 1024);

	// detect windows
	if(iswinxp())
	{
		printf_c("You are running Windows 2000 or above, and IOCP support is most likely available.\n"
			     "This removes the socket number limitation of select and is much more efficent.\n"
				 "If you are unsure, answer yes.\n\n");

		use_iocp = get_bool_option("Do you want to use the IOCP implementation?", true);
	}

	support_ip6links = get_bool_option("\nYou have chosen to build an \033[1;32mIPV4-only\033[0m server.\nWould you like to enable support for linking to IPV6-enabled InspIRCd servers?\nIf you are using a recent operating system and are unsure, answer yes.\nIf you answer 'no' here, your InspIRCd server will be unable\nto parse IPV6 addresses (e.g. for CIDR bans)", 
		true);
	
	printf_c("\033[1mAll paths are relative to the binary directory.\033[0m\n");
	get_string_option("In what directory do you wish to install the InspIRCd base?", "..", base_path);
	get_string_option("In what directory are the configuration files?", "../conf", config_file);
	get_string_option("In what directory are the modules to be compiled to?", "../modules", mod_path);
	get_string_option("In what directory is the IRCd binary to be placed?", ".", bin_dir);
	get_string_option("In what directory are the IRCd libraries to be placed?", "../lib", library_dir);

	printf_c("The following questions will ask you for various figures relating\n"
		"To your IRCd install. Please note that these should usually be left\n"
		"as defaults unless you have a real reason to change them. If they\n"
		"changed, then the values must be identical on all servers on your\n"
		"network, or malfunctions and/or crashes may occur, with the exception\n"
		"of the 'maximum number of clients' setting which may be different on\n"
		"different servers on the network.\n\n");

    
	max_clients = get_int_option("Please enter the maximum number of clients at any one time?", 1024);
	nicklen = get_int_option("Please enter the maximum length of nicknames?", 31);
	chanlen = get_int_option("Please enter the maximum length of channel names?", 64);
	modechanges = get_int_option("Please enter the maximum number of mode changes in one line?", 20);
	identlen = get_int_option("Please enter the maximum length of an ident (username)?", 12);
	quitlen = get_int_option("Please enter the maximum length of a quit message?", 255);
	topiclen = get_int_option("Please enter the maximum length of a channel topic?", 307);
	kicklen = get_int_option("Please enter the maximum length of a kick message?", 255);
	rllen = get_int_option("Please enter the maximum length of a GECOS (real name)?", 128);
	awaylen = get_int_option("Please enter the maximum length of an away message?", 200);

	printf_c("\n\033[1;32mPre-build configuration is complete!\n\n");	sc(TNORMAL);

	// dump all the options back out
	printf_c("\033[0mBase install path:\033[1;32m        %s\n", base_path);
	printf_c("\033[0mConfig path:\033[1;32m              %s\n", config_file);
	printf_c("\033[0mModule path:\033[1;32m              %s\n", mod_path);
	printf_c("\033[0mLibrary path:\033[1;32m             %s\n", library_dir);
	printf_c("\033[0mSocket Engine:\033[1;32m            %s\n", use_iocp ? "iocp" : "select");
	printf_c("\033[0mMax file descriptors:\033[1;32m     %u\n", max_fd);
	printf_c("\033[0mMax connections:\033[1;32m          %u\n", max_clients);
	printf_c("\033[0mMax nickname length:\033[1;32m      %u\n", nicklen);
	printf_c("\033[0mMax channel length:\033[1;32m       %u\n", chanlen);
	printf_c("\033[0mMax mode length:\033[1;32m          %u\n", modechanges);
	printf_c("\033[0mMax ident length:\033[1;32m         %u\n", identlen);
	printf_c("\033[0mMax quit length:\033[1;32m          %u\n", quitlen);
	printf_c("\033[0mMax topic length:\033[1;32m         %u\n", topiclen);
	printf_c("\033[0mMax kick length:\033[1;32m          %u\n", kicklen);
	printf_c("\033[0mMax name length:\033[1;32m          %u\n", rllen);
	printf_c("\033[0mMax away length:\033[1;32m          %u\n", awaylen);
	printf("\n"); sc(TNORMAL);
	if(get_bool_option("Are these settings correct?", true) == false)
	{
		Run();
		return;
	}
	printf("\n");

	// escape the pathes
	escape_string(config_file, MAX_PATH);
	escape_string(mod_path, MAX_PATH);
	escape_string(library_dir, MAX_PATH);

	printf("\nWriting inspircd_config.h...");
	FILE * f = fopen("inspircd_config.h", "w");
	fprintf(f, "/* Auto generated by configure, do not modify! */\n");
	fprintf(f, "#ifndef __CONFIGURATION_AUTO__\n");
	fprintf(f, "#define __CONFIGURATION_AUTO__\n\n");
	if(use_iocp)
		fprintf(f, "#define CONFIG_USE_IOCP 1\n\n");

	fprintf(f, "#define CONFIG_FILE \"%s/inspircd.conf\"\n", config_file);
	fprintf(f, "#define MOD_PATH \"%s\"\n", mod_path);
	fprintf(f, "#define MAX_DESCRIPTORS %u\n", max_fd);
	fprintf(f, "#define MAXCLIENTS %u\n", max_clients);
	fprintf(f, "#define MAXCLIENTS_S \"%u\"\n", max_clients);
	fprintf(f, "#define SOMAXCONN_S \"128\"\n");
	fprintf(f, "#define NICKMAX %u\n", nicklen+1);
	fprintf(f, "#define CHANMAX %u\n", chanlen+1);
	fprintf(f, "#define MAXMODES %u\n", modechanges);
	fprintf(f, "#define IDENTMAX %u\n", identlen);
	fprintf(f, "#define MAXQUIT %u\n", quitlen);
	fprintf(f, "#define MAXTOPIC %u\n", topiclen);
	fprintf(f, "#define MAXKICK %u\n", kicklen);
	fprintf(f, "#define MAXGECOS %u\n", rllen);
	fprintf(f, "#define MAXAWAY %u\n", awaylen);
	fprintf(f, "#define LIBRARYDIR \"%s\"\n", library_dir);
	fprintf(f, "#define VERSION \"%s\"\n", version);
	fprintf(f, "#define REVISION \"%s\"\n", revision_text);
	if(support_ip6links)
		fprintf(f, "#define SUPPORT_IP6LINKS 1\n");

	OSVERSIONINFO vi;
	vi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	GetVersionEx(&vi);
#ifdef WIN64
	fprintf(f, "#define SYSTEM \"Windows_x64 %u.%u.%u %s\"\n", vi.dwMajorVersion, vi.dwMinorVersion, vi.dwBuildNumber, vi.szCSDVersion);
#else
	fprintf(f, "#define SYSTEM \"Windows_x32 %u.%u.%u %s\"\n", vi.dwMajorVersion, vi.dwMinorVersion, vi.dwBuildNumber, vi.szCSDVersion);
#endif
	fprintf(f, "#define MAXBUF 514\n");

	fprintf(f, "\n#include \"inspircd_win32wrapper.h\"\n\n");
	fprintf(f, "#endif\n\n");
	fclose(f);

	sc(TGREEN); printf(" done\n"); sc(TNORMAL);
	printf("Writing inspircd_se_config.h...");

	f = fopen("inspircd_se_config.h", "w");
	fprintf(f, "/* Auto generated by configure, do not modify or commit to svn! */\n");
	fprintf(f, "#ifndef __CONFIGURATION_SOCKETENGINE__\n");
	fprintf(f, "#define __CONFIGURATION_SOCKETENGINE__\n\n");
	fprintf(f, "#include \"socketengine_%s.h\"\n\n", use_iocp ? "iocp" : "select");
	fprintf(f, "#endif\n\n");
	fclose(f);

	sc(TGREEN); printf(" done\n"); sc(TNORMAL);
	printf("Writing command and module compilation scripts...");
	WriteCompileCommands();
	WriteCompileModules();
	sc(TGREEN); printf(" done\n"); sc(TNORMAL);

	printf("\nconfigure is done.. exiting!\n");
}

void WriteCompileCommands()
{
	char commands[300][100];
	int command_count = 0;
	printf("\n  Finding Command Sources...\n");
	WIN32_FIND_DATA fd;
	HANDLE fh = FindFirstFile("..\\src\\cmd_*.cpp", &fd);
	if(fh == INVALID_HANDLE_VALUE)
		printf_c("\033[1;32m  No command sources could be found! This \033[1m*could*\033[1;32m be a bad thing.. :P\033[0m");
	else
	{
		sc(TGREEN);
		do 
		{
			strcpy(commands[command_count], fd.cFileName);
			commands[command_count][strlen(fd.cFileName) - 4] = 0;
			printf("    %s\n", commands[command_count]);
			++command_count;
		} while(FindNextFile(fh, &fd));
		sc(TNORMAL);
	}
    
	// Write our spiffy new makefile :D
	// I am such a lazy fucker :P
	FILE * f = fopen("..\\src\\commands.mak", "w");

	time_t t = time(NULL);
	fprintf(f, "# Generated at %s\n", ctime(&t));
	fprintf(f, "all: makedir ");

	// dump modules.. first time :)
	for(int i = 0; i < command_count; ++i)
		fprintf(f, "%s.so ", commands[i]);

	fprintf(f, "\n.cpp.obj:\n");
#ifdef WIN64
	// /MACHINE:X64
	#ifdef _DEBUG
		fprintf(f, "  cl /nologo /LD /Od /I \".\" /I \"../include\" /I \"../include/modes\" /I \"../include/commands\" /I \"../win\" /D \"WIN32\" /D \"_DEBUG\" /D \"_CONSOLE\" /D \"_MBCS\" /D \"DLL_BUILD\" /Gm /EHsc /Gm /RTC1 /MTd /Fo\"Debug/\" /Fd\"Debug/vc70.pdb\" /W2 /Wp64 /Zi /TP $*.cpp ..\\win\\inspircd_memory_functions.cpp /link ..\\bin\\debug_x64\\bin\\inspircd.lib /OUT:\"..\\bin\\debug_x64\\lib\\$*.so\" /PDB:\"..\\bin\\debug_x64\\lib\\$*.pdb\" /MACHINE:X64 /IMPLIB:\"..\\bin\\debug_x64\\lib\\$*.lib\"\n\n");
		CreateDirectory("..\\src\\debug", NULL);
		CreateDirectory("..\\bin\\debug\\bin", NULL);
		CreateDirectory("..\\bin\\debug\\lib", NULL);
		CreateDirectory("..\\bin\\debug\\modules", NULL);
	#else
		fprintf(f, "  cl /nologo /LD /Od /I \".\" /I \"../include\" /I \"../include/modes\" /I \"../include/commands\" /I \"../win\" /D \"WIN32\" /D \"_CONSOLE\" /D \"_MBCS\" /D \"DLL_BUILD\" /EHsc /Gm /MT /Fo\"Release/\" /Fd\"Release/vc70.pdb\" /W2 /Wp64 /Zi /TP $*.cpp ..\\win\\inspircd_memory_functions.cpp /link ..\\bin\\release_x64\\bin\\inspircd.lib /OUT:\"..\\bin\\release_x64\\lib\\$*.so\" /PDB:\"..\\bin\\release_x64\\lib\\$*.pdb\" /MACHINE:X64 /IMPLIB:\"..\\bin\\release_x64\\lib\\$*.lib\"\n\n");
		CreateDirectory("..\\src\\release", NULL);
		CreateDirectory("..\\bin\\release\\bin", NULL);
		CreateDirectory("..\\bin\\release\\lib", NULL);
		CreateDirectory("..\\bin\\release\\modules", NULL);
	#endif
#else
	#ifdef _DEBUG
		fprintf(f, "  cl /nologo /LD /Od /I \".\" /I \"../include\" /I \"../include/modes\" /I \"../include/commands\" /I \"../win\" /D \"WIN32\" /D \"_DEBUG\" /D \"_CONSOLE\" /D \"_MBCS\" /D \"DLL_BUILD\" /Gm /EHsc /Gm /RTC1 /MTd /Fo\"Debug/\" /Fd\"Debug/vc70.pdb\" /W2 /Wp64 /Zi /TP $*.cpp ..\\win\\inspircd_memory_functions.cpp /link ..\\bin\\debug\\bin\\inspircd.lib /OUT:\"..\\bin\\debug\\lib\\$*.so\" /PDB:\"..\\bin\\debug\\lib\\$*.pdb\" /IMPLIB:\"..\\bin\\debug\\lib\\$*.lib\"\n\n");
		CreateDirectory("..\\src\\debug", NULL);
		CreateDirectory("..\\bin\\debug\\bin", NULL);
		CreateDirectory("..\\bin\\debug\\lib", NULL);
		CreateDirectory("..\\bin\\debug\\modules", NULL);
	#else
		fprintf(f, "  cl /nologo /LD /Od /I \".\" /I \"../include\" /I \"../include/modes\" /I \"../include/commands\" /I \"../win\" /D \"WIN32\" /D \"_CONSOLE\" /D \"_MBCS\" /D \"DLL_BUILD\" /EHsc /Gm /MT /Fo\"Release/\" /Fd\"Release/vc70.pdb\" /W2 /Wp64 /Zi /TP $*.cpp ..\\win\\inspircd_memory_functions.cpp /link ..\\bin\\release\\bin\\inspircd.lib /OUT:\"..\\bin\\release\\lib\\$*.so\" /PDB:\"..\\bin\\release\\lib\\$*.pdb\" /IMPLIB:\"..\\bin\\release\\lib\\$*.lib\"\n\n");
		CreateDirectory("..\\src\\release", NULL);
		CreateDirectory("..\\bin\\release\\bin", NULL);
		CreateDirectory("..\\bin\\release\\lib", NULL);
		CreateDirectory("..\\bin\\release\\modules", NULL);
	#endif
#endif

	fprintf(f, "makedir:\n  if not exist debug mkdir debug\n\n");
	
	// dump modules.. again the second and last time :)
	for(int i = 0; i < command_count; ++i)
		fprintf(f, "%s.so : %s.obj\n", commands[i], commands[i]);

	fprintf(f, "\n");
	fclose(f);
}

void WriteCompileModules()
{
	char modules[300][100];
	int module_count = 0;

	printf("Finding Modules...\n");
	WIN32_FIND_DATA fd;
	HANDLE fh = FindFirstFile("..\\src\\modules\\m_*.cpp", &fd);
	if(fh == INVALID_HANDLE_VALUE)
		printf_c("\033[1;32m  No module sources could be found! This \033[1m*could*\033[1;32m be a bad thing.. :P\033[0m");
	else
	{
		sc(TGREEN);
		do 
		{
			strcpy(modules[module_count], fd.cFileName);
			modules[module_count][strlen(fd.cFileName) - 4] = 0;
			printf("  %s\n", modules[module_count]);
			++module_count;
		} while(FindNextFile(fh, &fd));
		sc(TNORMAL);
	}

	// Write our spiffy new makefile :D
	// I am such a lazy fucker :P
	FILE * f = fopen("..\\src\\modules\\modules.mak", "w");

	time_t t = time(NULL);
	fprintf(f, "# Generated at %s\n", ctime(&t));
	fprintf(f, "all: makedir ");

	// dump modules.. first time :)
	for(int i = 0; i < module_count; ++i)
		fprintf(f, "%s.so ", modules[i]);

	fprintf(f, "\n.cpp.obj:\n");
#ifdef WIN64
	// /MACHINE:X64
	#ifdef _DEBUG
		fprintf(f, "  cl /nologo /LD /Od /I \".\" /I \"../../include\" /I \"../../include/modes\" /I \"../../include/modules\" /I \"../../win\" /D \"WIN32\" /D \"_DEBUG\" /D \"_CONSOLE\" /D \"_MBCS\" /D \"DLL_BUILD\" /Gm /EHsc /Gm /RTC1 /MTd /Fo\"Debug/\" /Fd\"Debug/vc70.pdb\" /W2 /Wp64 /Zi /TP $*.cpp ..\\..\\win\\inspircd_memory_functions.cpp /link ..\\..\\bin\\debug_x64\\bin\\inspircd.lib ws2_32.lib /OUT:\"..\\..\\bin\\debug_x64\\modules\\$*.so\" /PDB:\"..\\..\\bin\\debug_x64\\modules\\$*.pdb\" /MACHINE:X64 /IMPLIB:\"..\\..\\bin\\debug_x64\\modules\\$*.lib\"\n\n");
		CreateDirectory("..\\src\\modules\\debug_x64", NULL);
	#else
		fprintf(f, "  cl /nologo /LD /Od /I \".\" /I \"../../include\" /I \"../../include/modes\" /I \"../../include/modules\" /I \"../../win\" /D \"WIN32\" /D \"_CONSOLE\" /D \"_MBCS\" /D \"DLL_BUILD\" /EHsc /Gm /MT /Fo\"Release/\" /Fd\"Release/vc70.pdb\" /W2 /Wp64 /Zi /TP $*.cpp ..\\..\\win\\inspircd_memory_functions.cpp /link ..\\..\\bin\\release_x64\\bin\\inspircd.lib ws2_32.lib /OUT:\"..\\..\\bin\\release_x64\\modules\\$*.so\" /PDB:\"..\\..\\bin\\release_x64\\modules\\$*.pdb\" /MACHINE:X64 /IMPLIB:\"..\\..\\bin\\release_x64\\modules\\$*.lib\"\n\n");
		CreateDirectory("..\\src\\modules\\release_x64", NULL);
	#endif
#else
	#ifdef _DEBUG
		fprintf(f, "  cl /nologo -Dssize_t=long /LD /Od /I \".\" /I \"../../include\" /I \"../../include/modes\" /I \"../../include/modules\" /I \"../../win\" /D \"WIN32\" /D \"_DEBUG\" /D \"_CONSOLE\" /D \"_MBCS\" /D \"DLL_BUILD\" /Gm /EHsc /Gm /RTC1 /MTd /Fo\"Debug/\" /Fd\"Debug/vc70.pdb\" /W2 /Wp64 /Zi /TP $*.cpp ..\\..\\win\\inspircd_memory_functions.cpp /link ..\\..\\bin\\debug\\bin\\inspircd.lib ws2_32.lib /OUT:\"..\\..\\bin\\debug\\modules\\$*.so\" /PDB:\"..\\..\\bin\\debug\\modules\\$*.pdb\" /IMPLIB:\"..\\..\\bin\\debug\\modules\\$*.lib\"\n\n");
		CreateDirectory("..\\src\\modules\\debug", NULL);
	#else
		fprintf(f, "  cl /nologo -Dssize_t=long /LD /Od /I \".\" /I \"../../include\" /I \"../../include/modes\" /I \"../../include/modules\" /I \"../../win\" /D \"WIN32\" /D \"_CONSOLE\" /D \"_MBCS\" /D \"DLL_BUILD\" /EHsc /Gm /MT /Fo\"Release/\" /Fd\"Release/vc70.pdb\" /W2 /Wp64 /Zi /TP $*.cpp ..\\..\\win\\inspircd_memory_functions.cpp /link ..\\..\\bin\\release\\bin\\inspircd.lib ws2_32.lib /OUT:\"..\\..\\bin\\release\\modules\\$*.so\" /PDB:\"..\\..\\bin\\release\\modules\\$*.pdb\" /IMPLIB:\"..\\..\\bin\\release\\modules\\$*.lib\"\n\n");
		CreateDirectory("..\\src\\modules\\release", NULL);
	#endif
#endif
	
	fprintf(f, "makedir:\n  if not exist debug mkdir debug\n\n");

	// dump modules.. again the second and last time :)
	for(int i = 0; i < module_count; ++i)
		fprintf(f, "%s.so : %s.obj\n", modules[i], modules[i]);

	fprintf(f, "\n");
	fclose(f);
}
