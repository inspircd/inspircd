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

using namespace std;

#include "inspircd.h" 
#include "inspircd_io.h" 
#include "inspircd_util.h" 
#include "inspstring.h"
#include "helperfuncs.h"

extern time_t TIME;
 
char *SafeStrncpy (char *dest, const char *src, size_t size) 
{ 
  if (!dest) 
    { 
      dest = NULL; 
      return (NULL); 
    } 
  else if (size < 1) 
    { 
      dest = NULL; 
      return (NULL); 
    } 
 
  memset (dest, '\0', size); 
  strlcpy (dest, src, size - 1);
 
  return (dest); 
} 
 
 
char *CleanIpAddr (char *cleanAddr, const char *dirtyAddr) 
{ 
  int count = 0, maxdot = 0, maxoctet = 0; 
 
  memset (cleanAddr, '\0', MAXBUF); 
  if(dirtyAddr == NULL) 
	return(cleanAddr); 
 
  for (count = 0; count < MAXBUF - 1; count++) 
    { 
      if (isdigit (dirtyAddr[count])) 
	{ 
	  if (++maxoctet > 3) 
	    { 
	      cleanAddr[count] = '\0'; 
	      break; 
	    } 
	  cleanAddr[count] = dirtyAddr[count]; 
	} 
      else if (dirtyAddr[count] == '.') 
	{ 
	  if (++maxdot > 3) 
	    { 
	      cleanAddr[count] = '\0'; 
	      break; 
	    } 
	  maxoctet = 0; 
	  cleanAddr[count] = dirtyAddr[count]; 
	} 
      else 
	{ 
	  cleanAddr[count] = '\0'; 
	  break; 
	} 
    } 
 
  return (cleanAddr); 
} 
 
char* CleanFilename(char* name)
{
	char* p = name + strlen(name);
	while ((p != name) && (*p != '/'))
		p--;
	if ( p != name)
	{
 		return ++p;
 	}
 	else
 	{
 		return p;
 	}
}
 
