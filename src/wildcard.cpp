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

#include <string>
#include "inspircd_config.h"
#include "inspircd.h"
#include "inspstring.h"

void Delete(char* str,int pos)
{
	char moo[MAXBUF];
	strlcpy(moo,str,MAXBUF);
	moo[pos] = '\0';
	strlcpy(str,moo,MAXBUF);
	strlcat(str,moo+pos+1,MAXBUF);
}

void Insert(char* substr,char* str,int pos)
{
	std::string a = str;
	a.insert(pos,substr);
	strlcpy(str,a.c_str(),MAXBUF);
}


int MWC = 0;

bool match2(char* literal,char* mask)
{

char OldM[MAXBUF];
int I,I2;

if (MWC)
	return true;

int lenliteral = strlen(literal);

if ((strchr(mask,'*')==0) && (lenliteral != (strlen(mask))))
	return 0;
 I=0;
 I2=0;
 while (I < strlen(mask))
 {
   if (I2 >= lenliteral)
	   return 0;
 
   if ((mask[I]=='*') && (MWC==0))
   {
     strlcpy(OldM,mask,MAXBUF);
     
     Delete(mask,I);
     
     while (strlen(mask)<255)
     {
       match2(literal,mask);
       if (MWC==2)
	       return 1;

       Insert("?",mask,I);
     }
     strlcpy(mask,OldM,MAXBUF);
     Delete(mask,I);
     Insert("?",mask,I);
   }
   if (mask[I]=='?')
   {
     I++;
     I2++;
     continue;
   }
   if (mask[I] != literal[I2])
	   return 0;
   if (MWC)
	   return 1;
   I++;
   I2++;
 }
 if (lenliteral==strlen(mask))
		 MWC=2;

}

bool match(const char* literal, const char* mask)
{
	char L[10240];
	char M[10240];
	MWC = 0;
	strlcpy(L,literal,10240);
	strlcpy(M,mask,10240);
	strlower(L);
	strlower(M);
	// short circuit literals
	log(DEBUG,"Match '%s' to '%s'",L,M);
	if ((!strchr(M,'*')) && (!strchr(M,'?')))
	{
		if (!strcasecmp(L,M))
		{
			return true;
		}
	}
	match2(L,M);
	return (MWC == 2);
}

