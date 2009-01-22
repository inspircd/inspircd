#include "modules.h"
#define OVRREQID "Override Request"
class OVRrequest : public Request
{
public:
User * requser;
std::string reqtoken;
OVRrequest(Module* s, Module* d, User* src, const std::string &token)
        : Request(s, d, OVRREQID), reqtoken(token)
	{
		requser = src;
	}
};
