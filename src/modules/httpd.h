#include "base.h"

#ifndef __HTTPD_H__
#define __HTTPD_H__

HTTPRequest : public classbase
{
 protected:

	std::string type;
	std::string document;
	std::string ipaddr;

 public:

	void* opaque;

	HTTPRequest(const std::string &request_type, const std::string &uri, void* opaque, const std::string &ip)
		: type(request_type), document(uri), ipaddr(ip)
	{
	}

	std::string& GetType()
	{
		return type;
	}

	std::string& GetURI()
	{
		return document;
	}

	std::string& GetIP()
	{
		return ipaddr;
	}
}

#endif

//httpr(request_type,uri,headers,this,this->GetIP());

