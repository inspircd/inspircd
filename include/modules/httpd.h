/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2013, 2021-2023 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013, 2015 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 John Brooks <john@jbrooks.io>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006, 2008 Craig Edwards <brain@inspircd.org>
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


#pragma once

#include "timeutils.h"

class HTTPQueryParameters final
	: public insp::flat_multimap<std::string, std::string>
{
public:
	bool get(const std::string& key, std::string& value) const
	{
		const_iterator it = find(key);
		if (it == end())
			return false;

		value = it->second;
		return true;
	}

	std::string getString(const std::string& key, const std::string& def = "") const
	{
		std::string value;
		if (!get(key, value))
			return def;

		return value;
	}

	template <typename T>
	T getNum(const std::string& key, T def = 0) const
	{
		std::string value;
		if (!get(key, value))
			return def;

		return ConvToNum<T>(value);
	}

	unsigned long getDuration(const std::string& key, unsigned long def = 0) const
	{
		unsigned long value;
		if (!Duration::TryFrom(getString(key, "0"), value))
			return def;

		return value;
	}

	bool getBool(const std::string& key, bool def = false) const
	{
		return !!getNum<uint8_t>(key, def);
	}
};

struct HTTPRequestURI final
{
	std::string path;
	HTTPQueryParameters query_params;
	std::string fragment;
};

/** A modifiable list of HTTP header fields
 */
class HTTPHeaders final
{
protected:
	std::map<std::string, std::string> headers;
public:

	/** Set the value of a header
	 * Sets the value of the named header. If the header is already present, it will be replaced
	 */
	void SetHeader(const std::string& name, const std::string& data)
	{
		headers[name] = data;
	}

	/** Set the value of a header, only if it doesn't exist already
	 * Sets the value of the named header. If the header is already present, it will NOT be updated
	 */
	void CreateHeader(const std::string& name, const std::string& data)
	{
		if (!IsSet(name))
			SetHeader(name, data);
	}

	/** Remove the named header
	 */
	void RemoveHeader(const std::string& name)
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
	std::string GetHeader(const std::string& name)
	{
		std::map<std::string, std::string>::iterator it = headers.find(name);
		if (it == headers.end())
			return std::string();

		return it->second;
	}

	/** Check if the given header is specified
	 * @return true if the header is specified
	 */
	bool IsSet(const std::string& name)
	{
		std::map<std::string, std::string>::iterator it = headers.find(name);
		return (it != headers.end());
	}

	/** Get all headers, formatted by the HTTP protocol
	 * @return Returns all headers, formatted according to the HTTP protocol. There is no request terminator at the end
	 */
	std::string GetFormattedHeaders()
	{
		std::stringstream buf;
		for (const auto& [key, value] : headers)
			buf << key << ": " << value << "\r\n";
		return buf.str();
	}
};

class HttpServerSocket;

/** This class represents a HTTP request.
 */
class HTTPRequest final
{
protected:
	std::string type;
	std::string ipaddr;
	std::string postdata;
	HTTPRequestURI parseduri;

public:

	HTTPHeaders* headers;
	int errorcode;

	/** A socket pointer, which you must return in your HTTPDocument class
	 * if you reply to this request.
	 */
	HttpServerSocket* sock;

	/** Initialize HTTPRequest.
	 * This constructor is called by m_httpd.so to initialize the class.
	 * @param request_type The request type, e.g. GET, POST, HEAD
	 * @param parsed_uri The URI which was requested by the client.
	 * @param hdr The headers sent with the request
	 * @param socket The server socket which this request came in via.
	 * @param ip The IP address making the web request.
	 * @param pdata The post data (content after headers) received with the request, up to Content-Length in size
	 */
	HTTPRequest(const std::string& request_type, const HTTPRequestURI& parsed_uri, HTTPHeaders* hdr,
			HttpServerSocket* socket, const std::string& ip, const std::string& pdata)
		: type(request_type)
		, ipaddr(ip)
		, postdata(pdata)
		, parseduri(parsed_uri)
		, headers(hdr)
		, sock(socket)
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

	HTTPRequestURI& GetParsedURI()
	{
		return parseduri;
	}

	std::string& GetPath()
	{
		return GetParsedURI().path;
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

/** If you want to reply to HTTP requests, you must return a HTTPDocumentResponse to
 * the httpd module via the HTTPdAPI.
 * When you initialize this class you initialize it with all components required to
 * form a valid HTTP response: the document data and a response code.
 * You can add additional HTTP headers, if you want.
 */
class HTTPDocumentResponse final
{
public:
	/** Module that generated this reply
	 */
	Module* const module;

	std::stringstream* document;
	unsigned int responsecode;

	/** Any extra headers to include with the defaults
	 */
	HTTPHeaders headers;

	HTTPRequest& src;

	/** Initialize a HTTPDocumentResponse ready for sending to the httpd module.
	 * @param mod A pointer to the module who responded to the request
	 * @param req The request you obtained from the HTTPRequest at an earlier time
	 * @param doc A stringstream containing the document body
	 * @param response A valid HTTP/1.0 or HTTP/1.1 response code. The response text will be determined for you
	 * based upon the response code.
	 */
	HTTPDocumentResponse(Module* mod, HTTPRequest& req, std::stringstream* doc, unsigned int response)
		: module(mod)
		, document(doc)
		, responsecode(response)
		, src(req)
	{
	}
};

class HTTPdAPIBase
	: public DataProvider
{
public:
	HTTPdAPIBase(Module* parent)
		: DataProvider(parent, "m_httpd_api")
	{
	}

	/** Answer an incoming HTTP request with the provided document
	 * @param response The response created by your module that will be sent to the client
	 */
	virtual void SendResponse(HTTPDocumentResponse& response) = 0;
};

/** The API provided by the httpd module that allows other modules to respond to incoming
 * HTTP requests
 */
class HTTPdAPI final
	: public dynamic_reference<HTTPdAPIBase>
{
public:
	HTTPdAPI(Module* parent)
		: dynamic_reference<HTTPdAPIBase>(parent, "m_httpd_api")
	{
	}
};

class HTTPACLEventListener
	: public Events::ModuleEventListener
{
public:
	HTTPACLEventListener(Module* mod, unsigned int eventprio = DefaultPriority)
		: ModuleEventListener(mod, "event/http-acl", eventprio)
	{
	}

	virtual ModResult OnHTTPACLCheck(HTTPRequest& req) = 0;
};

class HTTPRequestEventListener
	: public Events::ModuleEventListener
{
public:
	HTTPRequestEventListener(Module* mod, unsigned int eventprio = DefaultPriority)
		: ModuleEventListener(mod, "event/http-request", eventprio)
	{
	}

	virtual ModResult OnHTTPRequest(HTTPRequest& req) = 0;
};
