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

chanrec::SetCustomMode(char mode,bool mode_on)
{
}

chanrec::SetCustomModeParam(char mode,char* parameter,bool mode_on)
{
}



