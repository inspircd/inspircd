#include "inspircd_config.h" 
#include "channels.h"
#include "inspircd.h"
#include <stdio.h>
#include <string>
#include <vector>

using namespace std;

std::vector<ModeParameter> custom_mode_params;

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
		char temp[MAXBUF];
		int count = 0;
		for (int q = 0; q < strlen(custom_modes); q++) {
			if (custom_modes[q] != mode) {
				temp[count++] = mode;
			}
		}
		temp[count] = '\0';
		strncpy(custom_modes,temp,MAXMODES);
		log(DEBUG,"Custom mode %c removed",mode);
		this->SetCustomModeParam(mode,"",false);
	}
}

void chanrec::SetCustomModeParam(char mode,char* parameter,bool mode_on)
{

	log(DEBUG,"SetCustomModeParam called");
	ModeParameter M;
	M.mode = mode;
	strcpy(M.channel,this->name);
	strcpy(M.parameter,parameter);
	if (mode_on)
	{
		log(DEBUG,"Custom mode parameter %c %s added",mode,parameter);
		custom_mode_params.push_back(M);
	}
	else
	{
		if (custom_mode_params.size())
		{
			for (vector<ModeParameter>::iterator i = custom_mode_params.begin(); i < custom_mode_params.end(); i++)
			{
				if ((i->mode == mode) && (!strcasecmp(this->name,i->channel)))
				{
					log(DEBUG,"Custom mode parameter %c %s removed",mode,parameter);
					custom_mode_params.erase(i);
					return;
				}
			}
		}
		log(DEBUG,"*** BUG *** Attempt to remove non-existent mode parameter!");
	}
}

bool chanrec::IsCustomModeSet(char mode)
{
	log(DEBUG,"Checking ISCustomModeSet: %c %s",mode,this->custom_modes);
	return (strchr(this->custom_modes,mode) != 0);
}

std::string chanrec::GetModeParameter(char mode)
{
	if (custom_mode_params.size())
	{
		for (vector<ModeParameter>::iterator i = custom_mode_params.begin(); i < custom_mode_params.end(); i++)
		{
			if ((i->mode == mode) && (!strcasecmp(this->name,i->channel)))
			{
				return std::string(i->parameter);
			}
		}
	}
	return std::string("");
}
