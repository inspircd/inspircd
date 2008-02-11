/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2008 InspIRCd Development Team
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
const char* HTTP_CLIENT_ERROR = "HTTPCLIENT_ERROR";

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
	HTTPClientRequest(InspIRCd *SI, Module *m, Module* target, const std::string &surl)
		: Request(m, target, HTTP_CLIENT_REQUEST), url(surl), Instance(SI), src(m)
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

	void AddHeader(std::string &header, std::string &sdata)
	{
		Headers[header] = sdata;
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

class HTTPClientError : public Request
{
 protected:
	friend class HTTPSocket;
	std::string url;
	int response;
	std::string responsestr;
	HeaderMap Headers;
 public:
	HTTPClientError(Module* src, Module* target, const std::string &surl, int iresponse)
		: Request(src, target, HTTP_CLIENT_ERROR), url(surl), response(iresponse)
	{
	}

	const std::string &GetURL()
	{
		return url;
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
	HTTPClientResponse(Module* src, Module* target, std::string &surl, int iresponse, std::string sresponsestr)
		: Request(src, target, HTTP_CLIENT_RESPONSE), url(surl), response(iresponse), responsestr(sresponsestr)
	{
	}

	HTTPClientResponse() : Request(NULL, NULL, HTTP_CLIENT_RESPONSE)
	{
	}

	void SetData(const std::string &ndata)
	{
		data = ndata;
	}
	
	void AddHeader(const std::string &header, const std::string &sdata)
	{
		Headers[header] = sdata;
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

	void SetResponse(const std::string &str)
	{
		responsestr = str;
		response = atoi(responsestr.c_str());
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
