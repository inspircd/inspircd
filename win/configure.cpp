/*	   +------------------------------------+
 *	   | Inspire Internet Relay Chat Daemon |
 *	   +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *			the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#define _CRT_SECURE_NO_DEPRECATE

#define CONFIGURE_BUILD
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <process.h>
#include <string>
#include <time.h>
#include "inspircd_win32wrapper.h"
#include "colours.h"

using namespace std;
void Run();
void Banner();
void WriteCompileModules();
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
	if (*def)
		printf_c("%s\n[\033[1;32m%s\033[0m] -> ", text, def);
	else
		printf_c("%s\n[] -> ", text);
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
	int rev = 0;
	
	FILE * f = fopen("..\\.svn\\entries", "r");
	if (f)
	{
		for (int q = 0; q < 4; ++q)
			fgets(buf, 1000, f);

		rev = atoi(buf);
		sprintf(buffer, "%u", rev);
		fclose(f);
	}
	
	return rev;
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
		while (fgets(buf2, 500, f)) { }
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
	bool use_openssl = false;
	bool ipv6 = true;
	char mod_path[MAX_PATH];
	char config_file[MAX_PATH];
	char library_dir[MAX_PATH];
	char base_path[MAX_PATH];
	char bin_dir[MAX_PATH];
	char revision_text[MAX_PATH];
	char openssl_inc_path[MAX_PATH];
	char openssl_lib_path[MAX_PATH];
	int revision = get_svn_revision(revision_text, MAX_PATH);
	char version[514];
	char machine_text[MAX_PATH];
	get_machine_info(machine_text, MAX_PATH);

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

	ipv6 = get_bool_option("Do you want to enable IPv6?", false);

	printf_c("\033[1mAll paths are relative to the binary directory.\033[0m\n");
	get_string_option("In what directory do you wish to install the InspIRCd base?", "..", base_path);
	get_string_option("In what directory are the configuration files?", "../conf", config_file);
	get_string_option("In what directory are the modules to be compiled to?", "../modules", mod_path);
	get_string_option("In what directory is the IRCd binary to be placed?", ".", bin_dir);
	get_string_option("In what directory are the IRCd libraries to be placed?", "../lib", library_dir);

	// NOTE: this may seem hackish (generating a batch build script), but it assures the user knows
	// what they're doing, and we don't have to mess with copying files and changing around modules.mak
	// for the extra libraries. --fez
	// in case it exists, remove old m_ssl_openssl.cpp
	remove("..\\src\\modules\\m_ssl_openssl.cpp");
	printf_c("You can compile InspIRCd modules that add OpenSSL or GnuTLS support for SSL functionality.\n"
		"To do so you will need the appropriate link libraries and header files on your system.\n");
	use_openssl = get_bool_option("Would you like to compile the IRCd with OpenSSL support?", false);
	if (use_openssl)
	{
		get_string_option("Please enter the full path to your OpenSSL include directory\n"
			"(e.g., C:\\openssl\\include, but NOT the openssl subdirectory under include\\)\n"
			"(also, path should not end in '\\')",
			"C:\\openssl\\include", openssl_inc_path);

		// NOTE: if inspircd ever changes so that it compiles with /MT instead of the /MTd switch, then
		// the dependency on libeay32mtd.lib and ssleay32mtd.lib will change to just libeay32.lib and
		// ssleay32.lib. --fez

		get_string_option("Please enter the full path to your OpenSSL library directory\n"
			"(e.g., C:\\openssl\\lib, which should contain libeay32mtd.lib and ssleay32mtd.lib)",
			"C:\\openssl\\lib", openssl_lib_path);

		// write batch file
		FILE *fp = fopen("compile_openssl.bat", "w");
		fprintf(fp, "@echo off\n");
		fprintf(fp, "echo This batch script compiles m_ssl_openssl for InspIRCd.\n");
		fprintf(fp, "echo NOTE: this batch file should be invoked from the Visual Studio Command Prompt (vsvars32.bat)\n");
		fprintf(fp, "set OPENSSL_INC_PATH=\"%s\"\n", openssl_inc_path);
		fprintf(fp, "set OPENSSL_LIB_PATH=\"%s\"\n", openssl_lib_path);
		fprintf(fp, "set COMPILE=cl /nologo /LD /Od /I \".\" /I \"../../include\" /I \"../../include/modes\" /I \"../../include/modules\" /I \"../../win\" /D \"WIN32\" /D \"_CONSOLE\" /D \"_MBCS\" /D \"DLL_BUILD\" /Gm /EHsc /MD /Fo\"Release/\" /Fd\"Release/vc90.pdb\" /W2 /Wp64 /Zi /TP /I %%OPENSSL_INC_PATH%% m_ssl_openssl.cpp ..\\..\\win\\inspircd_memory_functions.cpp %%OPENSSL_INC_PATH%%\\openssl\\applink.c /link /LIBPATH:%%OPENSSL_LIB_PATH%% ..\\..\\bin\\release\\bin\\inspircd.lib ws2_32.lib /OUT:\"..\\..\\bin\\release\\modules\\m_ssl_openssl.so\" /PDB:\"..\\..\\bin\\release\\modules\\m_ssl_openssl.pdb\" /IMPLIB:\"..\\..\\bin\\release\\modules\\m_ssl_openssl.lib\"\n");
		fprintf(fp, "cd ..\\src\\modules\n");
		fprintf(fp, "copy extra\\m_ssl_openssl.cpp .\n");
		fprintf(fp, "echo \t%%COMPILE%%\n");
		fprintf(fp, "%%COMPILE%%\n");
		fprintf(fp, "cd ..\\..\\win\n");
		fprintf(fp, "echo done... now check for errors.\n");
		fclose(fp);

		printf_c("\033[1;32m!!!NOTICE!!! The file 'compile_openssl.bat' has been written to your 'win' directory.  Launch it\n"
			"!!! from the Visual Studio Command Prompt !!! to compile the m_ssl_openssl module.\n"
			"Wait until after compiling inspircd to run it.\n"
			"Also, ssleay32.dll and libeay32.dll will be required for the IRCd to run.\033[0m\n");
	}

	printf_c("\n\033[1;32mPre-build configuration is complete!\n\n");	sc(TNORMAL);

	CopyExtras();

	// dump all the options back out
	printf_c("\033[0mBase install path:\033[1;32m        %s\n", base_path);
	printf_c("\033[0mConfig path:\033[1;32m              %s\n", config_file);
	printf_c("\033[0mModule path:\033[1;32m              %s\n", mod_path);
	printf_c("\033[0mLibrary path:\033[1;32m             %s\n", library_dir);
	printf_c("\033[0mSocket Engine:\033[1;32m            %s\n", "select");

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

	fprintf(f, "#define CONFIG_FILE \"%s/inspircd.conf\"\n", config_file);
	fprintf(f, "#define MOD_PATH \"%s\"\n", mod_path);
	fprintf(f, "#define SOMAXCONN_S \"128\"\n");
	fprintf(f, "#define LIBRARYDIR \"%s\"\n", library_dir);
	fprintf(f, "#define MAXBUF 514\n");

	fprintf(f, "\n#include \"inspircd_win32wrapper.h\"");
	fprintf(f, "\n#include \"inspircd_namedpipe.h\"");
	fprintf(f, "\n#include \"threadengines/threadengine_win32.h\"\n\n");
	fprintf(f, "#endif\n\n");
	fclose(f);

	sc(TGREEN); printf(" done\n"); sc(TNORMAL);
	printf("Writing inspircd_se_config.h...");

	f = fopen("inspircd_se_config.h", "w");
	fprintf(f, "/* Auto generated by configure, do not modify or commit to svn! */\n");
	fprintf(f, "#ifndef __CONFIGURATION_SOCKETENGINE__\n");
	fprintf(f, "#define __CONFIGURATION_SOCKETENGINE__\n\n");
	fprintf(f, "#include \"socketengines/socketengine_%s.h\"\n\n", "select");
	fprintf(f, "#endif\n\n");
	fclose(f);

	sc(TGREEN); printf(" done\n"); sc(TNORMAL);
	printf("Writing inspircd_version.h...");
	f = fopen("inspircd_version.h", "w");
	fprintf(f, "#define VERSION \"%s\"\n", version);
	fprintf(f, "#define REVISION \"%d\"\n", revision);
	fprintf(f, "#define SYSTEM \"%s\"\n", machine_text);
	fclose(f);

	sc(TGREEN); printf(" done\n"); sc(TNORMAL);
	printf("Writing command and module compilation scripts...");
	WriteCompileCommands();
	WriteCompileModules();
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

	*dest = 0;

	WIN32_FIND_DATA fd;
#ifdef _DEBUG
	HANDLE fh = FindFirstFile("..\\bin\\debug\\lib\\*.so", &fd);
#else
	HANDLE fh = FindFirstFile("..\\bin\\release\\lib\\*.so", &fd);
#endif
	if(fh == INVALID_HANDLE_VALUE)
		return;

	do
	{
#ifdef _DEBUG
		strcat(dest, " ..\\bin\\debug\\lib\\");
#else
		strcat(dest, " ..\\bin\\release\\lib\\");
#endif
		strcat(dest, fd.cFileName);
	}
	while (FindNextFile(fh, &fd));

	FindClose(fh);

	sprintf(command, "rebase.exe -v -b 10000000 -c baseaddr_commands.txt %s", dest);
	printf("%s\n", command);
	system(command);

#ifdef _DEBUG
	fh = FindFirstFile("..\\bin\\debug\\modules\\*.so", &fd);
#else
	fh = FindFirstFile("..\\bin\\release\\modules\\*.so", &fd);
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
		fprintf(f, "  cl /nologo /LD /Od /I \".\" /I \"../../include\" /I \"../../include/modes\" /I \"../../include/commands\" /I \"../../win\" /D \"WIN32\" /D \"_DEBUG\" /D \"_CONSOLE\" /D \"_MBCS\" /D \"DLL_BUILD\" /Gm /EHsc /RTC1 /MDd /Fo\"Debug/\" /Fd\"Debug/vc90.pdb\" /W2 /Zi /TP $*.cpp ..\\..\\win\\inspircd_memory_functions.cpp /link ..\\..\\bin\\debug_x64\\bin\\inspircd.lib /OUT:\"..\\..\\bin\\debug_x64\\lib\\$*.so\" /PDB:\"..\\..\\bin\\debug_x64\\lib\\$*.pdb\" /MACHINE:X64 /IMPLIB:\"..\\..\\bin\\debug_x64\\lib\\$*.lib\"\n\n");
		CreateDirectory("..\\src\\debug", NULL);
		CreateDirectory("..\\bin\\debug\\bin", NULL);
		CreateDirectory("..\\bin\\debug\\lib", NULL);
		CreateDirectory("..\\bin\\debug\\modules", NULL);
	#else
		fprintf(f, "  cl /nologo /LD /Od /I \".\" /I \"../../include\" /I \"../../include/modes\" /I \"../../include/commands\" /I \"../../win\" /D \"WIN32\" /D \"_CONSOLE\" /D \"_MBCS\" /D \"DLL_BUILD\" /Gm /EHsc /GL /MD /Fo\"Release/\" /Fd\"Release/vc90.pdb\" /W2 /Zi /TP $*.cpp ..\\..\\win\\inspircd_memory_functions.cpp /link ..\\..\\bin\\release_x64\\bin\\inspircd.lib /OUT:\"..\\..\\bin\\release_x64\\lib\\$*.so\" /PDB:\"..\\..\\bin\\release_x64\\lib\\$*.pdb\" /MACHINE:X64 /IMPLIB:\"..\\..\\bin\\release_x64\\lib\\$*.lib\"\n\n");
		CreateDirectory("..\\src\\release", NULL);
		CreateDirectory("..\\bin\\release\\bin", NULL);
		CreateDirectory("..\\bin\\release\\lib", NULL);
		CreateDirectory("..\\bin\\release\\modules", NULL);
	#endif
#else
	#ifdef _DEBUG
		fprintf(f, "  cl /nologo /LD /Od /I \".\" /I \"../../include\" /I \"../../include/modes\" /I \"../../include/commands\" /I \"../../win\" /D \"WIN32\" /D \"_DEBUG\" /D \"_CONSOLE\" /D \"_MBCS\" /D \"DLL_BUILD\" /Gm /EHsc /RTC1 /MDd /Fo\"Debug/\" /Fd\"Debug/vc90.pdb\" /W2 /Zi /TP $*.cpp ..\\..\\win\\inspircd_memory_functions.cpp /link ..\\..\\bin\\debug\\bin\\inspircd.lib /OUT:\"..\\..\\bin\\debug\\lib\\$*.so\" /PDB:\"..\\..\\bin\\debug\\lib\\$*.pdb\" /IMPLIB:\"..\\..\\bin\\debug\\lib\\$*.lib\"\n\n");
		CreateDirectory("..\\src\\debug", NULL);
		CreateDirectory("..\\bin\\debug\\bin", NULL);
		CreateDirectory("..\\bin\\debug\\lib", NULL);
		CreateDirectory("..\\bin\\debug\\modules", NULL);
	#else
		fprintf(f, "  cl /nologo /LD /Od /I \".\" /I \"../../include\" /I \"../../include/modes\" /I \"../../include/commands\" /I \"../../win\" /D \"WIN32\" /D \"_CONSOLE\" /D \"_MBCS\" /D \"DLL_BUILD\" /Gm /EHsc /GL /MD /Fo\"Release/\" /Fd\"Release/vc90.pdb\" /W2 /Zi /TP $*.cpp ..\\..\\win\\inspircd_memory_functions.cpp /link ..\\..\\bin\\release\\bin\\inspircd.lib /OUT:\"..\\..\\bin\\release\\lib\\$*.so\" /PDB:\"..\\..\\bin\\release\\lib\\$*.pdb\" /IMPLIB:\"..\\..\\bin\\release\\lib\\$*.lib\"\n\n");
		CreateDirectory("..\\src\\release", NULL);
		CreateDirectory("..\\bin\\release\\bin", NULL);
		CreateDirectory("..\\bin\\release\\lib", NULL);
		CreateDirectory("..\\bin\\release\\modules", NULL);
	#endif
#endif

#ifdef _DEBUG
	fprintf(f, "makedir:\n  if not exist debug mkdir debug\n  if not exist ..\\..\\bin\\debug\\lib mkdir ..\\..\\bin\\debug\\lib\n\n");
#else
	fprintf(f, "makedir:\n  if not exist release mkdir release\n  if not exist ..\\..\\bin\\release\\lib mkdir ..\\..\\bin\\release\\lib\n\n");
#endif
	
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
		fprintf(f, "  cl /nologo /LD /Od /I \".\" /I \"../../include\" /I \"../../include/modes\" /I \"../../include/modules\" /I \"../../win\" /D \"WIN32\" /D \"_DEBUG\" /D \"_CONSOLE\" /D \"_MBCS\" /D \"DLL_BUILD\" /Gm /EHsc /RTC1 /MDd /Fo\"Debug/\" /Fd\"Debug/vc90.pdb\" /W2 /Zi /TP $*.cpp ..\\..\\win\\inspircd_memory_functions.cpp /link ..\\..\\bin\\debug_x64\\bin\\inspircd.lib ws2_32.lib /OUT:\"..\\..\\bin\\debug_x64\\modules\\$*.so\" /PDB:\"..\\..\\bin\\debug_x64\\modules\\$*.pdb\" /MACHINE:X64 /IMPLIB:\"..\\..\\bin\\debug_x64\\modules\\$*.lib\"\n\n");
		CreateDirectory("..\\src\\modules\\debug_x64", NULL);
	#else
		fprintf(f, "  cl /nologo /LD /Od /I \".\" /I \"../../include\" /I \"../../include/modes\" /I \"../../include/modules\" /I \"../../win\" /D \"WIN32\" /D \"_CONSOLE\" /D \"_MBCS\" /D \"DLL_BUILD\" /Gm /EHsc /GL /MD /Fo\"Release/\" /Fd\"Release/vc90.pdb\" /W2 /Zi /TP $*.cpp ..\\..\\win\\inspircd_memory_functions.cpp /link ..\\..\\bin\\release_x64\\bin\\inspircd.lib ws2_32.lib /OUT:\"..\\..\\bin\\release_x64\\modules\\$*.so\" /PDB:\"..\\..\\bin\\release_x64\\modules\\$*.pdb\" /MACHINE:X64 /IMPLIB:\"..\\..\\bin\\release_x64\\modules\\$*.lib\"\n\n");
		CreateDirectory("..\\src\\modules\\release_x64", NULL);
	#endif
#else
	#ifdef _DEBUG
		fprintf(f, "  cl /nologo /LD /Od /I \".\" /I \"../../include\" /I \"../../include/modes\" /I \"../../include/modules\" /I \"../../win\" /D \"WIN32\" /D \"_DEBUG\" /D \"_CONSOLE\" /D \"_MBCS\" /D \"DLL_BUILD\" /Gm /EHsc /RTC1 /MDd /Fo\"Debug/\" /Fd\"Debug/vc90.pdb\" /W2 /Zi /TP $*.cpp ..\\..\\win\\inspircd_memory_functions.cpp /link ..\\..\\bin\\debug\\bin\\inspircd.lib ws2_32.lib /OUT:\"..\\..\\bin\\debug\\modules\\$*.so\" /PDB:\"..\\..\\bin\\debug\\modules\\$*.pdb\" /IMPLIB:\"..\\..\\bin\\debug\\modules\\$*.lib\"\n\n");
		CreateDirectory("..\\src\\modules\\debug", NULL);
		CreateDirectory("..\\src\\modules\\debug\\lib", NULL);
		CreateDirectory("..\\src\\modules\\debug\\modules", NULL);
		CreateDirectory("..\\src\\modules\\debug\\bin", NULL);
	#else
		fprintf(f, "  cl /nologo /LD /Od /I \".\" /I \"../../include\" /I \"../../include/modes\" /I \"../../include/modules\" /I \"../../win\" /D \"WIN32\" /D \"_CONSOLE\" /D \"_MBCS\" /D \"DLL_BUILD\" /Gm /EHsc /GL /MD /Fo\"Release/\" /Fd\"Release/vc90.pdb\" /W2 /Zi /TP $*.cpp ..\\..\\win\\inspircd_memory_functions.cpp /link ..\\..\\bin\\release\\bin\\inspircd.lib ws2_32.lib /OUT:\"..\\..\\bin\\release\\modules\\$*.so\" /PDB:\"..\\..\\bin\\release\\modules\\$*.pdb\" /IMPLIB:\"..\\..\\bin\\release\\modules\\$*.lib\"\n\n");
		CreateDirectory("..\\src\\modules\\release", NULL);
		CreateDirectory("..\\src\\modules\\release\\lib", NULL);
		CreateDirectory("..\\src\\modules\\release\\modules", NULL);
		CreateDirectory("..\\src\\modules\\release\\bin", NULL);
	#endif
#endif
	
#ifdef _DEBUG
	fprintf(f, "makedir:\n  if not exist debug mkdir debug\n  if not exist ..\\..\\bin\\debug\\modules mkdir ..\\..\\bin\\debug\\modules\n\n");
#else
	fprintf(f, "makedir:\n  if not exist release mkdir release\n  if not exist ..\\..\\bin\\release\\modules mkdir ..\\..\\bin\\release\\modules\n\n");
#endif

	// dump modules.. again the second and last time :)
	for(int i = 0; i < module_count; ++i)
		fprintf(f, "%s.so : %s.obj\n", modules[i], modules[i]);

	fprintf(f, "\n");
	fclose(f);
}
