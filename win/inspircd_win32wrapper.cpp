/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2011 Adam <Adam@anope.org>
 *   Copyright (C) 2007, 2009 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007-2009 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2008 John Brooks <john.brooks@dereferenced.net>
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

#include "inspircd_win32wrapper.h"
#include "inspircd.h"
#include "configreader.h"
#include <string>
#include <errno.h>
#include <assert.h>

CoreExport const char *insp_inet_ntop(int af, const void *src, char *dst, socklen_t cnt)
{

	if (af == AF_INET)
	{
		struct sockaddr_in in;
		memset(&in, 0, sizeof(in));
		in.sin_family = AF_INET;
		memcpy(&in.sin_addr, src, sizeof(struct in_addr));
		getnameinfo((struct sockaddr *)&in, sizeof(struct sockaddr_in), dst, cnt, NULL, 0, NI_NUMERICHOST);
		return dst;
	}
	else if (af == AF_INET6)
	{
		struct sockaddr_in6 in;
		memset(&in, 0, sizeof(in));
		in.sin6_family = AF_INET6;
		memcpy(&in.sin6_addr, src, sizeof(struct in_addr6));
		getnameinfo((struct sockaddr *)&in, sizeof(struct sockaddr_in6), dst, cnt, NULL, 0, NI_NUMERICHOST);
		return dst;
	}
	return NULL;
}

CoreExport int insp_inet_pton(int af, const char *src, void *dst)
{
	sockaddr_in sa;
	int len = sizeof(SOCKADDR);
	int rv = WSAStringToAddressA((LPSTR)src, af, NULL, (LPSOCKADDR)&sa, &len);
	if(rv >= 0)
	{
		if(WSAGetLastError() == WSAEINVAL)
			rv = 0;
		else
			rv = 1;
	}
	memcpy(dst, &sa.sin_addr, sizeof(struct in_addr));
	return rv;
}

CoreExport DIR * opendir(const char * path)
{
	std::string search_path = std::string(path) + "\\*.*";
	WIN32_FIND_DATAA fd;
	HANDLE f = FindFirstFileA(search_path.c_str(), &fd);
	if (f != INVALID_HANDLE_VALUE)
	{
		DIR * d = new DIR;
		memcpy(&d->find_data, &fd, sizeof(WIN32_FIND_DATA));
		d->find_handle = f;
		d->first = true;
		return d;
	}
	else
	{
		return 0;
	}
}

CoreExport dirent * readdir(DIR * handle)
{
	if (handle->first)
		handle->first = false;
	else
	{
		if (!FindNextFileA(handle->find_handle, &handle->find_data))
			return 0;
	}

	strncpy(handle->dirent_pointer.d_name, handle->find_data.cFileName, MAX_PATH);
	return &handle->dirent_pointer;
}

CoreExport void closedir(DIR * handle)
{
	FindClose(handle->find_handle);
	delete handle;
}

int optind = 1;
char optarg[514];
int getopt_long(int ___argc, char *const *___argv, const char *__shortopts, const struct option *__longopts, int *__longind)
{
	// burlex todo: handle the shortops, at the moment it only works with longopts.

	if (___argc == 1 || optind == ___argc)			// No arguments (apart from filename)
		return -1;

	const char * opt = ___argv[optind];
	optind++;

	// if we're not an option, return an error.
	if (strnicmp(opt, "--", 2) != 0)
		return 1;
	else
		opt += 2;


	// parse argument list
	int i = 0;
	for (; __longopts[i].name != 0; ++i)
	{
		if (!strnicmp(__longopts[i].name, opt, strlen(__longopts[i].name)))
		{
			// woot, found a valid argument =)
			char * par = 0;
			if ((optind) != ___argc)
			{
				// grab the parameter from the next argument (if its not another argument)
				if (strnicmp(___argv[optind], "--", 2) != 0)
				{
//					optind++;		// Trash this next argument, we won't be needing it.
					par = ___argv[optind-1];
				}
			}			

			// increment the argument for next time
//			optind++;

			// determine action based on type
			if (__longopts[i].has_arg == required_argument && !par)
			{
				// parameter missing and its a required parameter option
				return 1;
			}

			// store argument in optarg
			if (par)
				strncpy(optarg, par, 514);

			if (__longopts[i].flag != 0)
			{
				// this is a variable, we have to set it if this argument is found.
				*__longopts[i].flag = 1;
				return 0;
			}
			else
			{
				if (__longopts[i].val == -1 || par == 0)
					return 1;
				
				return __longopts[i].val;
			}			
			break;
		}
	}

	// return 1 (invalid argument)
	return 1;
}

#include "../src/modules/m_spanningtree/link.h"
#include "../src/modules/ssl.h"
template class reference<Link>;
template class reference<Autoconnect>;
template class reference<ssl_cert>;
template class reference<OperInfo>;
