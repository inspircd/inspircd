/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Pippijn van Steenhoven <pip88nl@gmail.com>
 *   Copyright (C) 2007 John Brooks <john.brooks@dereferenced.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Craig Edwards <craigedwards@brainbox.cc>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "base.h"

#ifndef HTTPD_H
#define HTTPD_H

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

class HttpServerSocket;

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

	/** A socket pointer, which you must return in your HTTPDocument class
	 * if you reply to this request.
	 */
	HttpServerSocket* sock;

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
		HTTPHeaders* hdr, HttpServerSocket* socket, const std::string &ip, const std::string &pdata)
		: Event(me, eventid), type(request_type), document(uri), ipaddr(ip), postdata(pdata), headers(hdr), sock(socket)
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
class HTTPDocumentResponse : public Request
{
 public:
	std::stringstream* document;
	int responsecode;
	HTTPHeaders headers;
	HTTPRequest& src;

	/** Initialize a HTTPRequest ready for sending to m_httpd.so.
	 * @param opaque The socket pointer you obtained from the HTTPRequest at an earlier time
	 * @param doc A stringstream containing the document body
	 * @param response A valid HTTP/1.0 or HTTP/1.1 response code. The response text will be determined for you
	 * based upon the response code.
	 * @param extra Any extra headers to include with the defaults, seperated by carriage return and linefeed.
	 */
	HTTPDocumentResponse(Module* me, HTTPRequest& req, std::stringstream* doc, int response)
		: Request(me, req.source, "HTTP-DOC"), document(doc), responsecode(response), src(req)
	{
	}
};

#endif

