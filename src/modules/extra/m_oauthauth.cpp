/*
 * Authentication module for Lightbulb Crew OAuth2.
 * Takes:
 *       nick on the format uXaY where X is the userId and Y is the avatarId
 *       password an access_token which is valid for user/avatar X/Y
 */

#include "inspircd.h"
#include <curl/curl.h>
#include <curl/easy.h>
/* $ModDesc: Allow/Deny connections based on an OAuth2 tokens and a userId/avatarId combination */
/* $LinkerFlags: -lcurl */

enum AuthState {
	AUTH_STATE_NONE = 0,
	AUTH_STATE_BUSY = 1,
	AUTH_STATE_FAIL = 2
};

class ModuleOauthAuth : public Module
{
	LocalIntExt pendingExt;

	std::string killreason;
	std::string identityServer;

 public:
	ModuleOauthAuth() : pendingExt("oauthauth-wait", this)
	{
	}

	void init()
	{
	    CURLcode curlInit = curl_global_init(CURL_GLOBAL_ALL);
	    if (curlInit) {
	        //TODO: Check how to quit inspircd gracefully
	        exit(1);
	    }
	    
		ServerInstance->Modules->AddService(pendingExt);
		OnRehash(NULL);
		Implementation eventlist[] = { I_OnCheckReady, I_OnRehash, I_OnUserRegister };
		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));
	}

	void OnRehash(User* user)
	{
    	ConfigTag* conf = ServerInstance->Config->ConfValue("oauthauth");
		killreason = conf->getString("killreason");
		identityServer = conf->getString("identityServer");
	}

	ModResult OnUserRegister(LocalUser* user)
	{
		if (pendingExt.get(user))
			return MOD_RES_PASSTHRU;

        //TODO: Use of fields other than nick possible?
        std::string token = user->password; 
        std::string nick = user->nick;
        size_t uPos = nick.find_first_of('u');
        size_t aPos = nick.find_first_of('a');

        //TODO: Possible to change nick here to the avatarName?

        if (uPos == std::string::npos || aPos == std::string::npos) {
            //Nick not in correct format
        	pendingExt.set(user, AUTH_STATE_FAIL);            
        } else {
            uPos++;
            aPos++;
            std::string userId = nick.substr(uPos, aPos - uPos - 1);
            std::string avatarId = nick.substr(aPos);        
            //TODO: Sync call might not be ok - look into threading model...
    		//Consider doing this async?
            bool validToken = checkToken(identityServer, userId, avatarId, token);
            if (validToken) {
                pendingExt.set(user, AUTH_STATE_NONE);
            } else {
            	pendingExt.set(user, AUTH_STATE_FAIL);
            }        
        }
   		return MOD_RES_PASSTHRU;
	}

	ModResult OnCheckReady(LocalUser* user)
	{
		switch (pendingExt.get(user))
		{
			case AUTH_STATE_NONE:
				return MOD_RES_PASSTHRU;
			case AUTH_STATE_BUSY:
				return MOD_RES_DENY;
			case AUTH_STATE_FAIL:
				ServerInstance->Users->QuitUser(user, killreason);
				return MOD_RES_DENY;
		}
		return MOD_RES_PASSTHRU;
	}

	Version GetVersion()
	{
		return Version("Allow/Deny connections based on an OAuth2 tokens and a userId/avatarId combination", VF_VENDOR);
	}
	
	private:
	//TODO: Return the name that is to be used?
    //TODO: Test with SSL
    bool checkToken(const std::string& baseUrl, const std::string& userId, const std::string& avatarId, const std::string& token) {
        std::string url = baseUrl;
        url += "/oauth2/users/";
        url += userId;
        url += "/avatars/";
        url += avatarId;
        
        std::string authHeader = "Authorization: Bearer ";
        authHeader += token;
        
        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, authHeader.c_str());  
    
        CURL *curl = curl_easy_init();
        if (curl) {
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);        
            curl_easy_perform(curl);
            long statusCode;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &statusCode);
            /* always cleanup */
            curl_easy_cleanup(curl);
            if (statusCode > 200 && statusCode < 299)
                return true;        
        }
        return false;
    }
};

MODULE_INIT(ModuleOauthAuth)
