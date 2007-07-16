/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "base.h"

#ifndef HTTPCLIENT_H__
#define HTTPCLIENT_H__

#include <string>
#include <map>

typedef std::map<std::string,std::string> HeaderMap;

const char* HTTP_CLIENT_RESPONSE = "HTTPCLIENT_RESPONSE";
const char* HTTP_CLIENT_REQUEST = "HTTPCLIENT_REQUEST";

/** This class represents an outgoing HTTP request
 */
class HTTPClientRequest : public Request
{
 protected:
	std::string url;
	InspIRCd *Instance;
	Module *src;
	HeaderMap Headers;
 public:
	HTTPClientRequest(InspIRCd *Instance, Module *src, Module* target, const std::string &url)
		: Request(src, target, HTTP_CLIENT_REQUEST), url(url), Instance(Instance), src(src)
	{
		Headers["User-Agent"] = "InspIRCd (m_http_client.so)";
		Headers["Connection"] = "Close";
		Headers["Accept"] = "*/*";
	}

	HTTPClientRequest() : Request(NULL, NULL, HTTP_CLIENT_REQUEST)
	{
	}

	const std::string &GetURL()
	{
		return url;
	}

	void AddHeader(std::string &header, std::string &data)
	{
		Headers[header] = data;
	}
	
	void DeleteHeader(std::string &header)
	{
		Headers.erase(header);
	}

	HeaderMap GetHeaders()
	{
		return Headers;
	}
};

class HTTPClientResponse : public Request
{
 protected:
	friend class HTTPSocket;
	
	std::string url;
	std::string data;
	int response;
	std::string responsestr;
	HeaderMap Headers;
 public:
	HTTPClientResponse(Module* src, Module* target, std::string &url, int response, std::string responsestr)
		: Request(src, target, HTTP_CLIENT_RESPONSE), url(url), response(response), responsestr(responsestr)
	{
	}

	HTTPClientResponse() : Request(NULL, NULL, HTTP_CLIENT_RESPONSE)
	{
	}

	void SetData(const std::string &ndata)
	{
		data = ndata;
	}
	
	void AddHeader(const std::string &header, const std::string &data)
	{
		Headers[header] = data;
	}
	
	const std::string &GetURL()
	{
		return url;
	}
	
	const std::string &GetData()
	{
		return data;
	}
	
	int GetResponse(std::string &str)
	{
		str = responsestr;
		return response;
	}
	
	std::string GetHeader(const std::string &header)
	{
		HeaderMap::iterator i = Headers.find(header);
		
		if (i != Headers.end())
			return i->second;
		else
			return "";
	}
};

#endif
