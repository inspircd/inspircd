/*

$Log$
Revision 1.1  2003/01/23 19:45:58  brain
Initial revision

Revision 1.2  2003/01/15 22:49:18  brain
Added log macros

   
*/

#include "inspircd.h" 
#include "inspircd_io.h" 
#include "inspircd_util.h" 
 
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
  strncpy (dest, src, size - 1); 
 
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
 
 
 
