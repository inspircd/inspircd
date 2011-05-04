/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2011 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "base.h"

#ifndef __HTTPD_H__
#define __HTTPD_H__

#include <string>
#include <sstream>
#include <map>

/** A modifyable list of HTTP header fields
 */
class HTTPHeaders
{
 protected:
	std::map<std::string,std::string> headers;
 public:

	/** Set the value of a header
	 * Sets the value of the named header. If the header is already present, it will be replaced
	 */
	void SetHeader(const std::string &name, const std::string &data)
	{
		headers[name] = data;
	}

	/** Set the value of a header, only if it doesn't exist already
	 * Sets the value of the named header. If the header is already present, it will NOT be updated
	 */
	void CreateHeader(const std::string &name, const std::string &data)
	{
		if (!IsSet(name))
			SetHeader(name, data);
	}

	/** Remove the named header
	 */
	void RemoveHeader(const std::string &name)
	{
		headers.erase(name);
	}

	/** Remove all headers
	 */
	void Clear()
	{
		headers.clear();
	}

	/** Get the value of a header
	 * @return The value of the header, or an empty string
	 */
	std::string GetHeader(const std::string &name)
	{
		std::map<std::string,std::string>::iterator it = headers.find(name);
		if (it == headers.end())
			return std::string();

		return it->second;
	}

	/** Check if the given header is specified
	 * @return true if the header is specified
	 */
	bool IsSet(const std::string &name)
	{
		std::map<std::string,std::string>::iterator it = headers.find(name);
		return (it != headers.end());
	}

	/** Get all headers, formatted by the HTTP protocol
	 * @return Returns all headers, formatted according to the HTTP protocol. There is no request terminator at the end
	 */
	std::string GetFormattedHeaders()
	{
		std::string re;

		for (std::map<std::string,std::string>::iterator i = headers.begin(); i != headers.end(); i++)
			re += i->first + ": " + i->second + "\r\n";

		return re;
	}
};

class HTTPDocumentResponse;

/** This class represents a HTTP request.
 */
class HTTPRequest : public Event
{
 protected:
	std::string type;
	std::string document;
	std::string ipaddr;
	std::string postdata;

 public:

	HTTPHeaders *headers;
	int errorcode;

	/** Initialize HTTPRequest.
	 * This constructor is called by m_httpd.so to initialize the class.
	 * @param request_type The request type, e.g. GET, POST, HEAD
	 * @param uri The URI, e.g. /page
	 * @param hdr The headers sent with the request
	 * @param opaque An opaque pointer used internally by m_httpd, which you must pass back to the module in your reply.
	 * @param ip The IP address making the web request.
	 * @param pdata The post data (content after headers) received with the request, up to Content-Length in size
	 */
	HTTPRequest(Module* me, const std::string &eventid, const std::string &request_type, const std::string &uri,
		HTTPHeaders* hdr, const std::string &ip, const std::string &pdata)
		: Event(me, eventid), type(request_type), document(uri), ipaddr(ip), postdata(pdata), headers(hdr)
	{
	}

	/** Get the post data (request content).
	 * All post data will be returned, including carriage returns and linefeeds.
	 * @return The postdata
	 */
	std::string& GetPostData()
	{
		return postdata;
	}

	/** Get the request type.
	 * Any request type can be intercepted, even ones which are invalid in the HTTP/1.1 spec.
	 * @return The request type, e.g. GET, POST, HEAD
	 */
	std::string& GetType()
	{
		return type;
	}

	/** Get URI.
	 * The URI string (URL minus hostname and scheme) will be provided by this function.
	 * @return The URI being requested
	 */
	std::string& GetURI()
	{
		return document;
	}

	/** Get IP address of requester.
	 * The requesting system's ip address will be returned.
	 * @return The IP address as a string
	 */
	std::string& GetIP()
	{
		return ipaddr;
	}

	virtual void Respond(HTTPDocumentResponse&) = 0;
};

/** You must return a HTTPDocument to the httpd module by using the Request class.
 * When you initialize this class you may initialize it with all components required to
 * form a valid HTTP response, including document data, headers, and a response code.
 */
class HTTPDocumentResponse
{
 public:
	std::stringstream* document;
	int responsecode;
	HTTPHeaders headers;

	/** Initialize a HTTPRequest ready for sending to m_httpd.so.
	 * @param opaque The socket pointer you obtained from the HTTPRequest at an earlier time
	 * @param doc A stringstream containing the document body
	 * @param response A valid HTTP/1.0 or HTTP/1.1 response code. The response text will be determined for you
	 * based upon the response code.
	 * @param extra Any extra headers to include with the defaults, seperated by carriage return and linefeed.
	 */
	HTTPDocumentResponse(std::stringstream* doc, int response)
		: document(doc), responsecode(response)
	{
	}
};

#endif

