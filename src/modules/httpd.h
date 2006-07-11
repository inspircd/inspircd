#include "base.h"

#ifndef __HTTPD_H__
#define __HTTPD_H__

#include <string>
#include <sstream>

class HTTPRequest : public classbase
{
 protected:

	std::string type;
	std::string document;
	std::string ipaddr;
	std::stringstream* headers;

 public:

	void* sock;

	HTTPRequest(const std::string &request_type, const std::string &uri, std::stringstream* hdr, void* opaque, const std::string &ip)
		: type(request_type), document(uri), ipaddr(ip), headers(hdr), sock(opaque)
	{
	}

	std::stringstream* GetHeaders()
	{
		return headers;
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
};

#endif

//httpr(request_type,uri,headers,this,this->GetIP());

