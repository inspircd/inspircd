#include "inspircd_config.h" 
#include "channels.h"
#include "inspircd.h"
#include <stdio.h>

chanrec::chanrec()
{
	strcpy(name,"");
	strcpy(custom_modes,"");
	strcpy(topic,"");
	strcpy(setby,"");
	strcpy(key,"");
	created = topicset = limit = 0;
	topiclock = noexternal = inviteonly = moderated = secret = c_private = false;
}

void chanrec::SetCustomMode(char mode,bool mode_on)
{
	if (mode_on) {
		char m[3];
		m[0] = mode;
		m[1] = '\0';
		if (!strchr(this->custom_modes,mode))
		{
			strncat(custom_modes,m,MAXMODES);
		}
		log(DEBUG,"Custom mode %c set",mode);
	}
	else {
		char temp[MAXMODES];
		int count = 0;
		for (int q = 0; q < strlen(custom_modes); q++) {
			if (custom_modes[q] != mode) {
				temp[count++] = mode;
			}
		}
		temp[count] = '\0';
		strncpy(custom_modes,temp,MAXMODES);
		log(DEBUG,"Custom mode %c removed",mode);
	}
}

void chanrec::SetCustomModeParam(char mode,char* parameter,bool mode_on)
{
}



