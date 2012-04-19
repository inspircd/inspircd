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

#ifndef WIN32SERVICE_H
#define WIN32SERVICE_H

/* Hook for win32service.cpp to exit properly with the service specific error code */
void SetServiceStopped(int status);

/* Marks the service as running, not called until the config is parsed */
void SetServiceRunning();

#endif
