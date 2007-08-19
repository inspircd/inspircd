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

/** This class represents a HTTP request.
 * It will be sent to all modules as the data section of
 * an Event.
 */
class HTTPRequest : public classbase
{
 protected:
	std::string type;
	std::string document;
	std::string ipaddr;
	std::string postdata;
	
 public:

	HTTPHeaders *headers;
	
	/** A socket pointer, which you must return in your HTTPDocument class
	 * if you reply to this request.
	 */
	void* sock;

	/** Initialize HTTPRequest.
	 * This constructor is called by m_httpd.so to initialize the class.
	 * @param request_type The request type, e.g. GET, POST, HEAD
	 * @param uri The URI, e.g. /page
	 * @param hdr The headers sent with the request
	 * @param opaque An opaque pointer used internally by m_httpd, which you must pass back to the module in your reply.
	 * @param ip The IP address making the web request.
	 * @param pdata The post data (content after headers) received with the request, up to Content-Length in size
	 */
	HTTPRequest(const std::string &request_type, const std::string &uri, HTTPHeaders* hdr, void* opaque, const std::string &ip, const std::string &pdata)
		: type(request_type), document(uri), ipaddr(ip), postdata(pdata), headers(hdr), sock(opaque)
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
};

/** You must return a HTTPDocument to the httpd module by using the Request class.
 * When you initialize this class you may initialize it with all components required to
 * form a valid HTTP response, including document data, headers, and a response code.
 */
class HTTPDocument : public classbase
{
 protected:

	std::stringstream* document;
	int responsecode;

 public:

	HTTPHeaders headers;
	
	/** The socket pointer from an earlier HTTPRequest
	 */
	void* sock;

	/** Initialize a HTTPRequest ready for sending to m_httpd.so.
	 * @param opaque The socket pointer you obtained from the HTTPRequest at an earlier time
	 * @param doc A stringstream containing the document body
	 * @param response A valid HTTP/1.0 or HTTP/1.1 response code. The response text will be determined for you
	 * based upon the response code.
	 * @param extra Any extra headers to include with the defaults, seperated by carriage return and linefeed.
	 */
	HTTPDocument(void* opaque, std::stringstream* doc, int response) : document(doc), responsecode(response), sock(opaque)
	{
	}

	/** Get the document text.
	 * @return The document text
	 */
	std::stringstream* GetDocument()
	{
		return this->document;
	}

	/** Get the document size.
	 * @return the size of the document text in bytes
	 */
	unsigned long GetDocumentSize()
	{
		return this->document->str().length();
	}

	/** Get the response code.
	 * @return The response code
	 */
	int GetResponseCode()
	{
		return this->responsecode;
	}
};

#endif

