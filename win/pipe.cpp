/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 * POSIX emulation layer for Windows.
 *
 *   Copyright (C) 2012 Anope Team <team@anope.org>
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

#include <WinSock2.h>
#include <WS2tcpip.h>

int pipe(int fds[2])
{
	struct sockaddr_in localhost;

	if (inet_pton(AF_INET, "127.0.0.1", &localhost.sin_addr) != 1)
		return -1;

	int cfd = socket(AF_INET, SOCK_STREAM, 0), lfd = socket(AF_INET, SOCK_STREAM, 0);
	if (cfd == -1 || lfd == -1)
	{
		closesocket(cfd);
		closesocket(lfd);
		return -1;
	}

	if (bind(lfd, reinterpret_cast<struct sockaddr *>(&localhost), sizeof(localhost)) == -1)
	{
		closesocket(cfd);
		closesocket(lfd);
		return -1;
	}

	if (listen(lfd, 1) == -1)
	{
		closesocket(cfd);
		closesocket(lfd);
		return -1;
	}

	struct sockaddr_in lfd_addr;
	socklen_t sz = sizeof(lfd_addr);
	getsockname(lfd, reinterpret_cast<struct sockaddr *>(&lfd_addr), &sz);

	if (connect(cfd, reinterpret_cast<struct sockaddr *>(&lfd_addr), sizeof(lfd_addr)))
	{
		closesocket(cfd);
		closesocket(lfd);
		return -1;
	}

	int afd = accept(lfd, NULL, NULL);
	closesocket(lfd);
	if (afd == -1)
	{
		closesocket(cfd);
		return -1;
	}

	fds[0] = cfd;
	fds[1] = afd;
	
	return 0;
}

