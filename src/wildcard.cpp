#include <string>
#include "inspircd_config.h"
#include "inspircd.h"

void Delete(char* str,int pos)
{
	char moo[MAXBUF];
	strcpy(moo,str);
	moo[pos] = '\0';
	strcpy(str,moo);
	strcat(str,moo+pos+1);
}

void Insert(char* substr,char* str,int pos)
{
	std::string a = str;
	a.insert(pos,substr);
	strcpy(str,a.c_str());
}


int MWC = 0;

bool match2(char* literal,char* mask)
{

char OldM[MAXBUF];
int I,I2;

if (MWC)
	return true;

if ((strstr(mask,"*")==0) && (strlen(literal) != strlen(mask)))
	return 0;
 I=0;
 I2=0;
 while (I < strlen(mask))
 {
   if (I2 >= strlen(literal))
	   return 0;
 
   if ((mask[I]=='*') && (MWC==0))
   {
     strcpy(OldM,mask);
     
     Delete(mask,I);
     
     while (strlen(mask)<255)
     {
       match2(literal,mask);
       if (MWC==2)
	       return 1;

       Insert("?",mask,I);
     }
     strcpy(mask,OldM);
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
 if (strlen(literal)==strlen(mask))
		 MWC=2;

}

bool match(const char* literal, const char* mask)
{
	char L[10240];
	char M[10240];
	MWC = 0;
	strncpy(L,literal,10240);
	strncpy(M,mask,10240);
	strlower(L);
	strlower(M);
	match2(L,M);
	return (MWC == 2);
}

