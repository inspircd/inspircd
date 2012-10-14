/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2011 Adam <Adam@anope.org>
 *   Copyright (C) 2007, 2009 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2007-2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2008 Eric Dietz <root@wrongway.org>
 *   Copyright (C) 2007 Burlex <???@???>
 *   Copyright (C) 2007 Robin Burchell <robin+git@viroteck.net>
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


#define _CRT_SECURE_NO_DEPRECATE

#define CONFIGURE_BUILD
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <process.h>
#include "../include/consolecolors.h"

WORD g_wOriginalColors;
WORD g_wBackgroundColor;
HANDLE g_hStdout;

#include <iostream>
#include <string>
#include <vector>
#include <time.h>
#include "inspircd_win32wrapper.h"

using namespace std;
void Run();
void Banner();
void WriteCompileModules(const vector<string> &, const vector<string> &);
void WriteCompileCommands();
void CopyExtras();

#ifdef _WIN64
	// /MACHINE:X64
	#ifdef _DEBUG
		#define OUTFOLDER "debug_x64"	
	#else
		#define OUTFOLDER "release_x64"	
	#endif
#else
	#ifdef _DEBUG
		#define OUTFOLDER "debug"	
	#else
		#define OUTFOLDER "release"	
	#endif
#endif

int get_int_option(const char * text, int def)
{
	static char buffer[500];
	int ret;
	std::cout << text << std::endl << " [" << con_green << def << con_reset << "] -> ";
	fgets(buffer, sizeof(buffer), stdin);
	if(sscanf(buffer, "%u", &ret) != 1)
		ret = def;

	std::cout << std::endl;
	return ret;
}

bool get_bool_option(const char * text, bool def)
{
	static char buffer[500];
	char ret[100];
	std::cout << text << " [" << con_green << (def ? 'y' : 'n') << con_reset << "] -> ";
	fgets(buffer, sizeof(buffer), stdin);
	if(sscanf(buffer, "%s", ret) != 1)
		strcpy(ret, def ? "y" : "n");

	std::cout << std::endl;
	return !strnicmp(ret, "y", 1);
}

string get_string_option(const char * text, char * def)
{
	if (def && *def)
		std::cout << text << std::endl << "[" << con_green << def << con_reset << "] -> ";
	else
		std::cout << text << std::endl << "[] -> ";
	
	char buffer[1000], buf[1000];
	fgets(buffer, sizeof(buffer), stdin);
	if (sscanf(buffer, "%s", buf) != 1)
		strcpy(buf, def);
	
	std::cout << std::endl;
	return buf;
}

// escapes a string for use in a c++ file
void escape_string(string &str)
{
	string copy = str;
	str.clear();
	
	for (unsigned i = 0; i < copy.size(); ++i)
	{
		str += copy[i];
		if (copy[i] == '\\')
			str += '\\';
	}
}

string get_git_commit()
{
	char buf[128];
	char *ref = NULL, *commit = NULL;
	FILE *f = fopen("../.git/HEAD", "r");
	if (f)
	{
		if (fgets(buf, sizeof(buf), f))
		{
			while (isspace(buf[strlen(buf) - 1]))
				buf[strlen(buf) - 1] = 0;
			char *p = strchr(buf, ' ');
			if (p)
				ref = ++p;
		}
		fclose(f);
	}
	if (ref == NULL)
		return "";
	string ref_file = string("../.git/") + string(ref);
	f = fopen(ref_file.c_str(), "r");
	if (f)
	{
		if (fgets(buf, sizeof(buf), f))
		{
			while (isspace(buf[strlen(buf) - 1]))
				buf[strlen(buf) - 1] = 0;
			commit = buf;
		}
		fclose(f);
	}

	return commit != NULL ? commit : "0";
}

void get_machine_info(char * buffer, size_t len)
{
	char buf[500];
	char buf2[500];

	DWORD dwSize = sizeof(buf);
	if (!GetComputerNameExA((COMPUTER_NAME_FORMAT)ComputerNameDnsFullyQualified, buf, &dwSize))
		sprintf(buf, "%s", "unknown");

	FILE * f = fopen("ver.txt.tmp", "r");
	if (f)
	{
		while (fgets(buf2, sizeof(buf2), f)) { }
		fclose(f);
		_unlink("ver.txt.tmp");
	}
	else
		sprintf(buf2, "%s", "unknown");

	sprintf(buffer, "%s ", buf);
	//strip newlines
	char* b = buffer + strlen(buf)+1;
	char *b2 = buf2;
	while (*b2)
	{
		if (*b2 != 10 && *b2 != 13)
		{
			*b = *b2;
			b++;
		}
		*b2++;
	}
	*b = 0;
}

vector<string> get_dir_list(const string &path_list)
{
	char *paths = _strdup(path_list.c_str());
	char *paths_save = paths;
	char *p = paths;
	vector<string> paths_return;

	while ((p = strchr(paths, ';')))
	{
		*p++ = 0;
		paths_return.push_back(paths);
		paths = p;
	}
	if (paths != NULL)
		paths_return.push_back(paths);
	free(paths_save);
	
	return paths_return;
}

int __stdcall WinMain(IN HINSTANCE hInstance, IN HINSTANCE hPrevInstance, IN LPSTR lpCmdLine, IN int nShowCmd )
{
	FILE * j = fopen("..\\include\\inspircd_config.h", "r");
	if (j)
	{
		if (MessageBoxA(0, "inspircd_config.h already exists. Remove it and build from clean?", "Configure program", MB_ICONQUESTION|MB_YESNO|MB_DEFBUTTON2) != IDYES)
		{
			fclose(j);
			exit(0);
		}
	}

	// call before we hook console handles
	system("ver > ver.txt.tmp");

	AllocConsole();

	// pipe standard handles to this console
	freopen("CONIN$", "r", stdin);
	freopen("CONOUT$", "w", stdout);
	freopen("CONOUT$", "w", stderr);

	// Initialize the console values
	g_hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO bufinf;
	if(GetConsoleScreenBufferInfo(g_hStdout, &bufinf))
	{
		g_wOriginalColors = bufinf.wAttributes & 0x00FF;
		g_wBackgroundColor = bufinf.wAttributes & 0x00F0;
	}
	else
	{
		g_wOriginalColors = FOREGROUND_RED|FOREGROUND_BLUE|FOREGROUND_GREEN;
		g_wBackgroundColor = 0;
	}

	Banner();
	Run();
	FreeConsole();
	return 0;
}

void Banner()
{
	std::cout << std::endl << "Welcome to the " << con_white_bright << "InspIRCd" << con_reset << " Configuration program! (" << con_white_bright << "interactive mode" << con_reset << ")" << std::endl
			 << con_white_bright << "Package maintainers: Type ./configure --help for non-interactive help" << con_reset << std::endl << std::endl
		     << "*** If you are unsure of any of these values, leave it blank for	***" << std::endl
			 << "*** standard settings that will work, and your server will run	  ***" << std::endl
			 << "*** using them. Please consult your IRC network admin if in doubt.  ***" << std::endl << std::endl
			 << "Press " << con_white_bright << "<RETURN>" << con_reset << " to accept the default for any option, or enter" << std::endl
			 << "a new value. Please note: You will " << con_white_bright << "HAVE" << con_reset << " to read the docs" << std::endl
			 << "dir, otherwise you won't have a config file!" << std::endl << std::endl;

}

void Run()
{
	vector<string> extra_include_paths, extra_lib_paths;
	string revision = get_git_commit();
	char version[514];
	char machine_text[MAX_PATH];
	get_machine_info(machine_text, MAX_PATH);

	// grab version
	FILE * fI = fopen("..\\src\\version.sh", "r");
	if(fI)
	{
		fgets(version, sizeof(version), fI);
		fgets(version, sizeof(version), fI);
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
		strcpy(version, "InspIRCd-0.0.0");
	
	string branch(version);
	branch.erase(branch.find_last_of('.'));
	
	std::cout << "Your operating system is: " << con_green << "Windows " <<
#ifdef _WIN64
	"x64 (64-bit)"
#else
	"x86 (32-bit)"
#endif
	<< con_reset << std::endl << "InspIRCd revision ID: " << con_green << ( (!revision.empty() && revision != "0") ? revision : "(Non-GIT build)" ) << con_reset << std::endl << std::endl
	<< con_white_bright << "Extra modules." << con_reset << std::endl;
	if (get_bool_option("Do you want to compile any extra non-core modules?", false))
	{
		string extra_i_path = get_string_option("Extra include search paths separate by \";\"", ".");
		string extra_l_path = get_string_option("Extra library search paths, separate by \";\"", ".");
		
		extra_include_paths = get_dir_list(extra_i_path);
		extra_lib_paths = get_dir_list(extra_l_path);
	}

	std::cout << con_white_bright << "All paths are relative to the binary directory." << con_reset << std::endl;
	string base_path = get_string_option("In what directory do you wish to install the InspIRCd base?", "..");
	string config_path = get_string_option("In what directory are the configuration files?", "conf");
	string mod_path = get_string_option("In what directory are the modules to be compiled to?", "modules");
	string data_path = get_string_option("In what directory is the variable data to be placed in?", "data");
	string log_path = get_string_option("In what directory is the logs to be placed in?", "logs");
	string bin_dir = get_string_option("In what directory is the IRCd binary to be placed?", ".");

	std::cout << std::endl << con_green << "Pre-build configuration is complete!" << std::endl << std::endl;

	CopyExtras();

	// dump all the options back out
	std::cout << con_reset << "Base install path:\t" << con_green << base_path << std::endl
	<< con_reset << "Config path:\t" << con_green << config_path << std::endl
	<< con_reset << "Module path:\t" << con_green << mod_path << std::endl
	<< con_reset << "Data path:\t"<< con_green << data_path << std::endl
	<< con_reset << "Log path:\t" << con_green << log_path << std::endl
	<< con_reset << "Socket Engine:\t" << con_green << "select" << con_reset << std::endl;

	if(get_bool_option("Are these settings correct?", true) == false)
	{
		Run();
		return;
	}
	std::cout << std::endl;

	// escape the pathes
	escape_string(data_path);
	escape_string(log_path);
	escape_string(config_path);
	escape_string(mod_path);

	printf("\nWriting inspircd_config.h...");
	FILE * f = fopen("..\\include\\inspircd_config.h", "w");
	fprintf(f, "/* Auto generated by configure, do not modify! */\n");
	fprintf(f, "#ifndef __CONFIGURATION_AUTO__\n");
	fprintf(f, "#define __CONFIGURATION_AUTO__\n\n");

	fprintf(f, "#define CONFIG_PATH \"%s\"\n", config_path.c_str());
	fprintf(f, "#define MOD_PATH \"%s\"\n", mod_path.c_str());
	fprintf(f, "#define DATA_PATH \"%s\"\n", data_path.c_str());
	fprintf(f, "#define LOG_PATH \"%s\"\n", log_path.c_str());
	fprintf(f, "#define SOMAXCONN_S \"128\"\n");
	fprintf(f, "#define MAXBUF 514\n");

	fprintf(f, "\n#include \"inspircd_win32wrapper.h\"");
	fprintf(f, "\n#include \"threadengines/threadengine_win32.h\"\n\n");
	fprintf(f, "#endif\n\n");
	fclose(f);

	std::cout << con_green << "done" << con_reset << std::endl;
	printf("Writing inspircd_version.h...");
	f = fopen("..\\include\\inspircd_version.h", "w");
	fprintf(f, "#define BRANCH \"%s\"\n", branch.c_str());
	fprintf(f, "#define VERSION \"%s\"\n", version);
	fprintf(f, "#define REVISION \"%s\"\n", revision.c_str());
	fprintf(f, "#define SYSTEM \"%s\"\n", machine_text);
	fclose(f);

	std::cout << con_green << "done" << con_reset << std::endl;
	printf("Writing command and module compilation scripts...");
	WriteCompileCommands();
	WriteCompileModules(extra_include_paths, extra_lib_paths);
	std::cout << con_green << "done" << con_reset << std::endl;

	printf("\nconfigure is done.. exiting!\n");
}

/* Keeps files from modules/extra up to date if theyre copied into modules/ */
void CopyExtras()
{
	char dest[65535];
	char src[65535];

	printf("\nUpdating extra modules in src/modules...\n");

	WIN32_FIND_DATAA fd;
	HANDLE fh = FindFirstFileA("..\\src\\modules\\extra\\*.*", &fd);

	if(fh == INVALID_HANDLE_VALUE)
		return;

	do
	{
		strcpy(dest, "..\\src\\modules\\");
		strcat(dest, fd.cFileName);
		strcpy(src, "..\\src\\modules\\extra\\");
		strcat(src, fd.cFileName);
		FILE* x = fopen(dest, "r");
		if (x)
		{
			fclose(x);
			CopyFileA(src, dest, false);
			std::cout << con_green << "\t" << fd.cFileName << con_reset << "..." << std::endl;
		}
	}
	while (FindNextFileA(fh, &fd));

	FindClose(fh);

	printf("\n\n");
}

void WriteCompileCommands()
{
	char commands[300][100];
	int command_count = 0;
	printf("\n  Finding Command Sources...\n");
	WIN32_FIND_DATAA fd;
	HANDLE fh = FindFirstFileA("..\\src\\commands\\cmd_*.cpp", &fd);
	if(fh == INVALID_HANDLE_VALUE)
		std::cout << con_green << "  No command sources could be found! This *could* be a bad thing.. :P" << con_reset << std::endl;
	else
	{
		std::cout << con_green;
		do 
		{
			strcpy(commands[command_count], fd.cFileName);
			commands[command_count][strlen(fd.cFileName) - 4] = 0;
			printf("	%s\n", commands[command_count]);
			++command_count;
		} while(FindNextFileA(fh, &fd));
		std::cout << con_reset;
	}
	
	// Write our spiffy new makefile :D
	// I am such a lazy fucker :P
	FILE * f = fopen("..\\src\\commands\\commands.mak", "w");

	time_t t = time(NULL);
	fprintf(f, "# Generated at %s\n", ctime(&t));
	fprintf(f, "all: makedir ");

	// dump modules.. first time :)
	for(int i = 0; i < command_count; ++i)
		fprintf(f, "%s.so ", commands[i]);

	fprintf(f, "\n.cpp.obj:\n");
#ifdef _WIN64
	// /MACHINE:X64
	#ifdef _DEBUG
		fprintf(f, "  cl /nologo /LD /Od /I \".\" /I \"../../include\" /I \"../../include/modes\" /I \"../../include/commands\" /I \"../../win\" /D \"_DEBUG\" /D \"_CONSOLE\" /D \"_MBCS\" /D \"DLL_BUILD\" /Gm /EHsc /RTC1 /MDd /Fo\"Debug_x64/\" /Fd\"Debug_x64/vc90.pdb\" /W2 /Zi /TP $*.cpp ..\\..\\win\\inspircd_memory_functions.cpp /link ..\\..\\bin\\debug_x64\\inspircd.lib /OUT:\"..\\..\\bin\\debug_x64\\modules\\$*.so\" /PDB:\"..\\..\\bin\\debug_x64\\modules\\$*.pdb\" /MACHINE:X64 /IMPLIB:\"..\\..\\bin\\debug_x64\\modules\\$*.lib\"\n\n");
	#else
		fprintf(f, "  cl /nologo /LD /O2 /I \".\" /I \"../../include\" /I \"../../include/modes\" /I \"../../include/commands\" /I \"../../win\" /D \"_CONSOLE\" /D \"_MBCS\" /D \"DLL_BUILD\" /Gm /EHsc /GL /MD /Fo\"Release_x64/\" /Fd\"Release_x64/vc90.pdb\" /W2 /Zi /TP $*.cpp ..\\..\\win\\inspircd_memory_functions.cpp /link ..\\..\\bin\\release_x64\\inspircd.lib /OUT:\"..\\..\\bin\\release_x64\\modules\\$*.so\" /PDB:\"..\\..\\bin\\release_x64\\modules\\$*.pdb\" /MACHINE:X64 /IMPLIB:\"..\\..\\bin\\release_x64\\modules\\$*.lib\"\n\n");
	#endif
#else
	#ifdef _DEBUG
		fprintf(f, "  cl /nologo /LD /Od /I \".\" /I \"../../include\" /I \"../../include/modes\" /I \"../../include/commands\" /I \"../../win\" /D \"_DEBUG\" /D \"_CONSOLE\" /D \"_MBCS\" /D \"DLL_BUILD\" /Gm /EHsc /RTC1 /MDd /Fo\"Debug/\" /Fd\"Debug/vc90.pdb\" /W2 /Zi /TP $*.cpp ..\\..\\win\\inspircd_memory_functions.cpp /link ..\\..\\bin\\debug\\inspircd.lib /OUT:\"..\\..\\bin\\debug\\modules\\$*.so\" /PDB:\"..\\..\\bin\\debug\\modules\\$*.pdb\" /IMPLIB:\"..\\..\\bin\\debug\\modules\\$*.lib\"\n\n");
	#else
		fprintf(f, "  cl /nologo /LD /O2 /I \".\" /I \"../../include\" /I \"../../include/modes\" /I \"../../include/commands\" /I \"../../win\" /D \"_CONSOLE\" /D \"_MBCS\" /D \"DLL_BUILD\" /Gm /EHsc /GL /MD /Fo\"Release/\" /Fd\"Release/vc90.pdb\" /W2 /Zi /TP $*.cpp ..\\..\\win\\inspircd_memory_functions.cpp /link ..\\..\\bin\\release\\inspircd.lib /OUT:\"..\\..\\bin\\release\\modules\\$*.so\" /PDB:\"..\\..\\bin\\release\\modules\\$*.pdb\" /IMPLIB:\"..\\..\\bin\\release\\modules\\$*.lib\"\n\n");
	#endif
#endif

	fprintf(f, "makedir:\n");

	CreateDirectoryA("..\\src\\commands\\" OUTFOLDER, NULL);
	CreateDirectoryA("..\\bin\\" OUTFOLDER "\\modules", NULL);
	fprintf(f, "	if not exist ..\\..\\bin\\" OUTFOLDER "\\modules mkdir ..\\..\\bin\\" OUTFOLDER "\\modules\n");
	fprintf(f, "	if not exist ..\\..\\bin\\" OUTFOLDER "\\data mkdir ..\\..\\bin\\" OUTFOLDER "\\data\n");
	fprintf(f, "	if not exist ..\\..\\bin\\" OUTFOLDER "\\logs mkdir ..\\..\\bin\\" OUTFOLDER "\\logs\n");
	
	// dump modules.. again the second and last time :)
	for(int i = 0; i < command_count; ++i)
		fprintf(f, "%s.so : %s.obj\n", commands[i], commands[i]);

	fprintf(f, "\n");
	fclose(f);
}

void WriteCompileModules(const vector<string> &includes, const vector<string> &libs)
{
	char modules[300][100];
	int module_count = 0;

	printf("Finding Modules...\n");
	WIN32_FIND_DATAA fd;
	HANDLE fh = FindFirstFileA("..\\src\\modules\\m_*.cpp", &fd);
	if(fh == INVALID_HANDLE_VALUE)
		std::cout << con_green << "  No module sources could be found! This *could* be a bad thing.. :P" << con_reset << std::endl;
	else
	{
		std::cout << con_green;
		do 
		{
			strcpy(modules[module_count], fd.cFileName);
			modules[module_count][strlen(fd.cFileName) - 4] = 0;
			printf("  %s\n", modules[module_count]);
			++module_count;
		} while(FindNextFileA(fh, &fd));
		std::cout << con_reset;
	}
	
	string extra_include, extra_lib;
	for (unsigned i = 0; i < includes.size(); ++i)
		extra_include += " /I \"" + includes[i] + "\" ";
	for (unsigned i = 0; i < libs.size(); ++i)
		extra_lib += " /LIBPATH:\"" + libs[i] + "\" ";

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
#ifdef _WIN64
	// /MACHINE:X64
	#ifdef _DEBUG
		fprintf(f, "  cl /nologo /LD /Od /I \".\" /I \"../../include\" /I \"../../include/modes\" /I \"../../include/modules\" /I \"../../win\" %s /D \"_DEBUG\" /D \"_CONSOLE\" /D \"_MBCS\" /D \"DLL_BUILD\" /Gm /EHsc /RTC1 /MDd /Fo\"Debug_x64/\" /Fd\"Debug_x64/vc90.pdb\" /W2 /Zi /TP $*.cpp ..\\..\\win\\inspircd_memory_functions.cpp /link %s ..\\..\\bin\\debug_x64\\inspircd.lib /OUT:\"..\\..\\bin\\debug_x64\\modules\\$*.so\" /PDB:\"..\\..\\bin\\debug_x64\\modules\\$*.pdb\" /MACHINE:X64 /IMPLIB:\"..\\..\\bin\\debug_x64\\modules\\$*.lib\"\n\n", extra_include.c_str(), extra_lib.c_str());
	#else
		fprintf(f, "  cl /nologo /LD /O2 /I \".\" /I \"../../include\" /I \"../../include/modes\" /I \"../../include/modules\" /I \"../../win\" %s /D \"_CONSOLE\" /D \"_MBCS\" /D \"DLL_BUILD\" /Gm /EHsc /GL /MD /Fo\"Release_x64/\" /Fd\"Release_x64/vc90.pdb\" /W2 /Zi /TP $*.cpp ..\\..\\win\\inspircd_memory_functions.cpp /link %s ..\\..\\bin\\release_x64\\inspircd.lib /OUT:\"..\\..\\bin\\release_x64\\modules\\$*.so\" /PDB:\"..\\..\\bin\\release_x64\\modules\\$*.pdb\" /MACHINE:X64 /IMPLIB:\"..\\..\\bin\\release_x64\\modules\\$*.lib\"\n\n", extra_include.c_str(), extra_lib.c_str());
	#endif
#else
	#ifdef _DEBUG
		fprintf(f, "  cl /nologo /LD /Od /I \".\" /I \"../../include\" /I \"../../include/modes\" /I \"../../include/modules\" /I \"../../win\" %s /D \"_DEBUG\" /D \"_CONSOLE\" /D \"_MBCS\" /D \"DLL_BUILD\" /Gm /EHsc /RTC1 /MDd /Fo\"Debug/\" /Fd\"Debug/vc90.pdb\" /W2 /Zi /TP $*.cpp ..\\..\\win\\inspircd_memory_functions.cpp /link %s ..\\..\\bin\\debug\\inspircd.lib /OUT:\"..\\..\\bin\\debug\\modules\\$*.so\" /PDB:\"..\\..\\bin\\debug\\modules\\$*.pdb\" /IMPLIB:\"..\\..\\bin\\debug\\modules\\$*.lib\"\n\n", extra_include.c_str(), extra_lib.c_str());
	#else
		fprintf(f, "  cl /nologo /LD /O2 /I \".\" /I \"../../include\" /I \"../../include/modes\" /I \"../../include/modules\" /I \"../../win\" %s /D \"_CONSOLE\" /D \"_MBCS\" /D \"DLL_BUILD\" /Gm /EHsc /GL /MD /Fo\"Release/\" /Fd\"Release/vc90.pdb\" /W2 /Zi /TP $*.cpp ..\\..\\win\\inspircd_memory_functions.cpp /link %s ..\\..\\bin\\release\\inspircd.lib /OUT:\"..\\..\\bin\\release\\modules\\$*.so\" /PDB:\"..\\..\\bin\\release\\modules\\$*.pdb\" /IMPLIB:\"..\\..\\bin\\release\\modules\\$*.lib\"\n\n", extra_include.c_str(), extra_lib.c_str());
	#endif
#endif

	CreateDirectoryA("..\\src\\modules\\" OUTFOLDER, NULL);
	
#ifdef _DEBUG
	fprintf(f, "makedir:\n  if not exist debug mkdir debug\n\n");
#else
	fprintf(f, "makedir:\n  if not exist release mkdir release\n\n");
#endif

	// dump modules.. again the second and last time :)
	for(int i = 0; i < module_count; ++i)
		fprintf(f, "%s.so : %s.obj\n", modules[i], modules[i]);

	fprintf(f, "\n");
	fclose(f);
}
