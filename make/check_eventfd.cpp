/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2012 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include <sys/eventfd.h>

int main() {
	eventfd_t efd_data;
	int fd;

	fd = eventfd(0, EFD_NONBLOCK);
	eventfd_read(fd, &efd_data);

	return (fd < 0);
}
