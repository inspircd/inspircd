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
#include <iostream>
#include <string>
#include <vector>
#include <time.h>
#include "inspircd_win32wrapper.h"
#include "colours.h"

using namespace std;
void Run();
void Banner();
void WriteCompileModules(const vector<string> &, const vector<string> &);
void WriteCompileCommands();
void Rebase();
void CopyExtras();

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
	fgets(buffer, sizeof(buffer), stdin);
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
	fgets(buffer, sizeof(buffer), stdin);
	if(sscanf(buffer, "%s", ret) != 1)
		strcpy(ret, def ? "y" : "n");

	printf("\n");
	return !strncmp(ret, "y", 1);
}

string get_string_option(const char * text, char * def)
{
	if (def && *def)
		printf_c("%s\n[\033[1;32m%s\033[0m] -> ", text, def);
	else
		printf_c("%s\n[] -> ", text);
	
	char buffer[1000], buf[1000];
	fgets(buffer, sizeof(buffer), stdin);
	if (sscanf(buffer, "%s", buf) != 1)
		strcpy(buf, def);
	
	printf("\n");
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

	return commit != NULL ? commit : "";
}

void get_machine_info(char * buffer, size_t len)
{
	char buf[500];
	char buf2[500];

	DWORD dwSize = sizeof(buf);
	if (!GetComputerNameEx((COMPUTER_NAME_FORMAT)ComputerNameDnsFullyQualified, buf, &dwSize))
		sprintf(buf, "%s", "unknown");

	FILE * f = fopen("ver.txt.tmp", "r");
	if (f)
	{
		while (fgets(buf2, sizeof(buf2), f)) { }
		fclose(f);
		unlink("ver.txt.tmp");
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
	char *paths = strdup(path_list.c_str());
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
	if (!strcmp(lpCmdLine, "/rebase"))
	{
		Rebase();
		return 0;
	}

	FILE * j = fopen("inspircd_config.h", "r");
	if (j)
	{
		if (MessageBox(0, "inspircd_config.h already exists. Remove it and build from clean?", "Configure program", MB_ICONQUESTION|MB_YESNO|MB_DEFBUTTON2) != IDYES)
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

	Banner();
	Run();
	FreeConsole();
	return 0;
}

void Banner()
{
	printf_c("\nWelcome to the \033[1mInspIRCd\033[0m Configuration program! (\033[1minteractive mode\033[0m)\n"
			 "\033[1mPackage maintainers: Type ./configure --help for non-interactive help\033[0m\n\n");
	printf_c("*** If you are unsure of any of these values, leave it blank for	***\n"
			 "*** standard settings that will work, and your server will run	  ***\n"
			 "*** using them. Please consult your IRC network admin if in doubt.  ***\n\n"
			 "Press \033[1m<RETURN>\033[0m to accept the default for any option, or enter\n"
			 "a new value. Please note: You will \033[1mHAVE\033[0m to read the docs\n"
			 "dir, otherwise you won't have a config file!\n\n");

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
	
#ifdef WIN64
	printf_c("Your operating system is: \033[1;32mwindows_x64 \033[0m\n");
#else
	printf_c("Your operating system is: \033[1;32mwindows_x32 \033[0m\n");
#endif
	printf_c("InspIRCd revision ID: \033[1;32m%s \033[0m\n\n", !revision.empty() ? revision.c_str() : "(Non-GIT build)");
	
	printf_c("\033[1mExtra modules.\033[0m\n");
	if (get_bool_option("Do you want to compile any extra non-core modules?", false))
	{
		string extra_i_path = get_string_option("Extra include search paths separate by \";\"", ".");
		string extra_l_path = get_string_option("Extra library search paths, separate by \";\"", ".");
		
		extra_include_paths = get_dir_list(extra_i_path);
		extra_lib_paths = get_dir_list(extra_l_path);
	}

	printf_c("\033[1mAll paths are relative to the binary directory.\033[0m\n");
	string base_path = get_string_option("In what directory do you wish to install the InspIRCd base?", "..");
	string config_file = get_string_option("In what directory are the configuration files?", "conf");
	string mod_path = get_string_option("In what directory are the modules to be compiled to?", "modules");
	string bin_dir = get_string_option("In what directory is the IRCd binary to be placed?", ".");

	printf_c("\n\033[1;32mPre-build configuration is complete!\n\n");	sc(TNORMAL);

	CopyExtras();

	// dump all the options back out
	printf_c("\033[0mBase install path:\033[1;32m        %s\n", base_path.c_str());
	printf_c("\033[0mConfig path:\033[1;32m              %s\n", config_file.c_str());
	printf_c("\033[0mModule path:\033[1;32m              %s\n", mod_path.c_str());
	printf_c("\033[0mSocket Engine:\033[1;32m            %s\n", "select");

	printf("\n"); sc(TNORMAL);
	if(get_bool_option("Are these settings correct?", true) == false)
	{
		Run();
		return;
	}
	printf("\n");

	// escape the pathes
	escape_string(config_file);
	escape_string(mod_path);

	printf("\nWriting inspircd_config.h...");
	FILE * f = fopen("inspircd_config.h", "w");
	fprintf(f, "/* Auto generated by configure, do not modify! */\n");
	fprintf(f, "#ifndef __CONFIGURATION_AUTO__\n");
	fprintf(f, "#define __CONFIGURATION_AUTO__\n\n");

	fprintf(f, "#define MOD_PATH \"%s\"\n", mod_path.c_str());
	fprintf(f, "#define SOMAXCONN_S \"128\"\n");
	fprintf(f, "#define MAXBUF 514\n");

	fprintf(f, "\n#include \"inspircd_win32wrapper.h\"");
	fprintf(f, "\n#include \"inspircd_namedpipe.h\"");
	fprintf(f, "\n#include \"threadengines/threadengine_win32.h\"\n\n");
	fprintf(f, "#endif\n\n");
	fclose(f);

	sc(TGREEN); printf(" done\n"); sc(TNORMAL);
	printf("Writing inspircd_se_config.h...");

	f = fopen("inspircd_se_config.h", "w");
	fprintf(f, "/* Auto generated by configure, do not modify or commit to Git! */\n");
	fprintf(f, "#ifndef __CONFIGURATION_SOCKETENGINE__\n");
	fprintf(f, "#define __CONFIGURATION_SOCKETENGINE__\n\n");
	fprintf(f, "#include \"socketengines/socketengine_%s.h\"\n\n", "select");
	fprintf(f, "#endif\n\n");
	fclose(f);

	sc(TGREEN); printf(" done\n"); sc(TNORMAL);
	printf("Writing inspircd_version.h...");
	f = fopen("inspircd_version.h", "w");
	fprintf(f, "#define BRANCH \"%s\"\n", branch.c_str());
	fprintf(f, "#define VERSION \"%s\"\n", version);
	fprintf(f, "#define REVISION \"%s\"\n", revision.c_str());
	fprintf(f, "#define SYSTEM \"%s\"\n", machine_text);
	fclose(f);

	sc(TGREEN); printf(" done\n"); sc(TNORMAL);
	printf("Writing command and module compilation scripts...");
	WriteCompileCommands();
	WriteCompileModules(extra_include_paths, extra_lib_paths);
	sc(TGREEN); printf(" done\n"); sc(TNORMAL);

	printf("\nconfigure is done.. exiting!\n");
}

/* Keeps files from modules/extra up to date if theyre copied into modules/ */
void CopyExtras()
{
	char dest[65535];
	char src[65535];

	printf("\nUpdating extra modules in src/modules...\n");

	WIN32_FIND_DATA fd;
	HANDLE fh = FindFirstFile("..\\src\\modules\\extra\\*.*", &fd);

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
			CopyFile(src, dest, false);
			sc(TGREEN); printf("    %s", fd.cFileName); sc(TNORMAL);
			printf("...\n");
		}
	}
	while (FindNextFile(fh, &fd));

	FindClose(fh);

	printf("\n\n");
}


void Rebase()
{
	char dest[65535];
	char command[65535];

	WIN32_FIND_DATA fd;

#ifdef _DEBUG
	HANDLE fh = FindFirstFile("..\\bin\\debug\\modules\\*.so", &fd);
#else
	HANDLE fh = FindFirstFile("..\\bin\\release\\modules\\*.so", &fd);
#endif
	if(fh == INVALID_HANDLE_VALUE)
		return;

	*dest = 0;

	do
	{
#ifdef _DEBUG
		strcat(dest, " ..\\bin\\debug\\modules\\");
#else
		strcat(dest, " ..\\bin\\release\\modules\\");
#endif
		strcat(dest, fd.cFileName);
	}
	while (FindNextFile(fh, &fd));

	sprintf(command, "rebase.exe -v -b 11000000 -c baseaddr_modules.txt %s", dest);
	printf("%s\n", command);
	system(command);

	FindClose(fh);
}

void WriteCompileCommands()
{
	char commands[300][100];
	int command_count = 0;
	printf("\n  Finding Command Sources...\n");
	WIN32_FIND_DATA fd;
	HANDLE fh = FindFirstFile("..\\src\\commands\\cmd_*.cpp", &fd);
	if(fh == INVALID_HANDLE_VALUE)
		printf_c("\033[1;32m  No command sources could be found! This \033[1m*could*\033[1;32m be a bad thing.. :P\033[0m");
	else
	{
		sc(TGREEN);
		do 
		{
			strcpy(commands[command_count], fd.cFileName);
			commands[command_count][strlen(fd.cFileName) - 4] = 0;
			printf("	%s\n", commands[command_count]);
			++command_count;
		} while(FindNextFile(fh, &fd));
		sc(TNORMAL);
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
#ifdef WIN64
	// /MACHINE:X64
	#ifdef _DEBUG
		fprintf(f, "  cl /nologo /LD /Od /I \".\" /I \"../../include\" /I \"../../include/modes\" /I \"../../include/commands\" /I \"../../win\" /D \"WIN32\" /D \"_DEBUG\" /D \"_CONSOLE\" /D \"_MBCS\" /D \"DLL_BUILD\" /Gm /EHsc /RTC1 /MDd /Fo\"Debug/\" /Fd\"Debug/vc90.pdb\" /W2 /Zi /TP $*.cpp ..\\..\\win\\inspircd_memory_functions.cpp /link ..\\..\\bin\\debug_x64\\inspircd.lib /OUT:\"..\\..\\bin\\debug_x64\\modules\\$*.so\" /PDB:\"..\\..\\bin\\debug_x64\\modules\\$*.pdb\" /MACHINE:X64 /IMPLIB:\"..\\..\\bin\\debug_x64\\modules\\$*.lib\"\n\n");
		CreateDirectory("..\\src\\commands\\debug", NULL);
		CreateDirectory("..\\bin\\debug\\modules", NULL);
	#else
		fprintf(f, "  cl /nologo /LD /Od /I \".\" /I \"../../include\" /I \"../../include/modes\" /I \"../../include/commands\" /I \"../../win\" /D \"WIN32\" /D \"_CONSOLE\" /D \"_MBCS\" /D \"DLL_BUILD\" /Gm /EHsc /GL /MD /Fo\"Release/\" /Fd\"Release/vc90.pdb\" /W2 /Zi /TP $*.cpp ..\\..\\win\\inspircd_memory_functions.cpp /link ..\\..\\bin\\release_x64\\inspircd.lib /OUT:\"..\\..\\bin\\release_x64\\modules\\$*.so\" /PDB:\"..\\..\\bin\\release_x64\\modules\\$*.pdb\" /MACHINE:X64 /IMPLIB:\"..\\..\\bin\\release_x64\\modules\\$*.lib\"\n\n");
		CreateDirectory("..\\src\\commands\\release", NULL);
		CreateDirectory("..\\bin\\release\\modules", NULL);
	#endif
#else
	#ifdef _DEBUG
		fprintf(f, "  cl /nologo /LD /Od /I \".\" /I \"../../include\" /I \"../../include/modes\" /I \"../../include/commands\" /I \"../../win\" /D \"WIN32\" /D \"_DEBUG\" /D \"_CONSOLE\" /D \"_MBCS\" /D \"DLL_BUILD\" /Gm /EHsc /RTC1 /MDd /Fo\"Debug/\" /Fd\"Debug/vc90.pdb\" /W2 /Zi /TP $*.cpp ..\\..\\win\\inspircd_memory_functions.cpp /link ..\\..\\bin\\debug\\inspircd.lib /OUT:\"..\\..\\bin\\debug\\modules\\$*.so\" /PDB:\"..\\..\\bin\\debug\\modules\\$*.pdb\" /IMPLIB:\"..\\..\\bin\\debug\\modules\\$*.lib\"\n\n");
		CreateDirectory("..\\src\\commands\\debug", NULL);
		CreateDirectory("..\\bin\\debug\\modules", NULL);
	#else
		fprintf(f, "  cl /nologo /LD /Od /I \".\" /I \"../../include\" /I \"../../include/modes\" /I \"../../include/commands\" /I \"../../win\" /D \"WIN32\" /D \"_CONSOLE\" /D \"_MBCS\" /D \"DLL_BUILD\" /Gm /EHsc /GL /MD /Fo\"Release/\" /Fd\"Release/vc90.pdb\" /W2 /Zi /TP $*.cpp ..\\..\\win\\inspircd_memory_functions.cpp /link ..\\..\\bin\\release\\inspircd.lib /OUT:\"..\\..\\bin\\release\\modules\\$*.so\" /PDB:\"..\\..\\bin\\release\\modules\\$*.pdb\" /IMPLIB:\"..\\..\\bin\\release\\modules\\$*.lib\"\n\n");
		CreateDirectory("..\\src\\commands\\release", NULL);
		CreateDirectory("..\\bin\\release\\modules", NULL);
	#endif
#endif

	fprintf(f, "makedir:\n");
#ifdef _DEBUG
	fprintf(f, "	if not exist ..\\..\\bin\\debug mkdir ..\\..\\bin\\debug\n");
	fprintf(f, "	if not exist ..\\..\\bin\\debug\\modules mkdir ..\\..\\bin\\debug\\modules\n");
	fprintf(f, "	if not exist ..\\..\\bin\\debug\\data mkdir ..\\..\\bin\\debug\\data\n");
	fprintf(f, "	if not exist ..\\..\\bin\\debug\\logs mkdir ..\\..\\bin\\debug\\logs\n");
#else
	fprintf(f, "	if not exist ..\\..\\bin\\release mkdir ..\\..\\bin\\release\n");
	fprintf(f, "	if not exist ..\\..\\bin\\release\\modules mkdir ..\\..\\bin\\release\\modules\n");
	fprintf(f, "	if not exist ..\\..\\bin\\release\\data mkdir ..\\..\\bin\\release\\data\n");
	fprintf(f, "	if not exist ..\\..\\bin\\release\\logs mkdir ..\\..\\bin\\release\\logs\n");
#endif
	
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
#ifdef WIN64
	// /MACHINE:X64
	#ifdef _DEBUG
		fprintf(f, "  cl /nologo /LD /Od /I \".\" /I \"../../include\" /I \"../../include/modes\" /I \"../../include/modules\" /I \"../../win\" %s /D \"WIN32\" /D \"_DEBUG\" /D \"_CONSOLE\" /D \"_MBCS\" /D \"DLL_BUILD\" /Gm /EHsc /RTC1 /MDd /Fo\"Debug/\" /Fd\"Debug/vc90.pdb\" /W2 /Zi /TP $*.cpp ..\\..\\win\\inspircd_memory_functions.cpp /link %s ..\\..\\bin\\debug_x64\\inspircd.lib ws2_32.lib /OUT:\"..\\..\\bin\\debug_x64\\modules\\$*.so\" /PDB:\"..\\..\\bin\\debug_x64\\modules\\$*.pdb\" /MACHINE:X64 /IMPLIB:\"..\\..\\bin\\debug_x64\\modules\\$*.lib\"\n\n", extra_include.c_str(), extra_lib.c_str());
		CreateDirectory("..\\src\\modules\\debug_x64", NULL);
	#else
		fprintf(f, "  cl /nologo /LD /Od /I \".\" /I \"../../include\" /I \"../../include/modes\" /I \"../../include/modules\" /I \"../../win\" %s /D \"WIN32\" /D \"_CONSOLE\" /D \"_MBCS\" /D \"DLL_BUILD\" /Gm /EHsc /GL /MD /Fo\"Release/\" /Fd\"Release/vc90.pdb\" /W2 /Zi /TP $*.cpp ..\\..\\win\\inspircd_memory_functions.cpp /link %s ..\\..\\bin\\release_x64\\inspircd.lib ws2_32.lib /OUT:\"..\\..\\bin\\release_x64\\modules\\$*.so\" /PDB:\"..\\..\\bin\\release_x64\\modules\\$*.pdb\" /MACHINE:X64 /IMPLIB:\"..\\..\\bin\\release_x64\\modules\\$*.lib\"\n\n", extra_include.c_str(), extra_lib.c_str());
		CreateDirectory("..\\src\\modules\\release_x64", NULL);
	#endif
#else
	#ifdef _DEBUG
		fprintf(f, "  cl /nologo /LD /Od /I \".\" /I \"../../include\" /I \"../../include/modes\" /I \"../../include/modules\" /I \"../../win\" %s /D \"WIN32\" /D \"_DEBUG\" /D \"_CONSOLE\" /D \"_MBCS\" /D \"DLL_BUILD\" /Gm /EHsc /RTC1 /MDd /Fo\"Debug/\" /Fd\"Debug/vc90.pdb\" /W2 /Zi /TP $*.cpp ..\\..\\win\\inspircd_memory_functions.cpp /link %s ..\\..\\bin\\debug\\inspircd.lib ws2_32.lib /OUT:\"..\\..\\bin\\debug\\modules\\$*.so\" /PDB:\"..\\..\\bin\\debug\\modules\\$*.pdb\" /IMPLIB:\"..\\..\\bin\\debug\\modules\\$*.lib\"\n\n", extra_include.c_str(), extra_lib.c_str());
		CreateDirectory("..\\src\\modules\\debug", NULL);
	#else
		fprintf(f, "  cl /nologo /LD /Od /I \".\" /I \"../../include\" /I \"../../include/modes\" /I \"../../include/modules\" /I \"../../win\" %s /D \"WIN32\" /D \"_CONSOLE\" /D \"_MBCS\" /D \"DLL_BUILD\" /Gm /EHsc /GL /MD /Fo\"Release/\" /Fd\"Release/vc90.pdb\" /W2 /Zi /TP $*.cpp ..\\..\\win\\inspircd_memory_functions.cpp /link %s ..\\..\\bin\\release\\inspircd.lib ws2_32.lib /OUT:\"..\\..\\bin\\release\\modules\\$*.so\" /PDB:\"..\\..\\bin\\release\\modules\\$*.pdb\" /IMPLIB:\"..\\..\\bin\\release\\modules\\$*.lib\"\n\n", extra_include.c_str(), extra_lib.c_str());
		CreateDirectory("..\\src\\modules\\release", NULL);
	#endif
#endif
	
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
