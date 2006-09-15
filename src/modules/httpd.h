#include "base.h"

#ifndef __HTTPD_H__
#define __HTTPD_H__

#include <string>
#include <sstream>

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
	std::stringstream* headers;

 public:

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
	HTTPRequest(const std::string &request_type, const std::string &uri, std::stringstream* hdr, void* opaque, const std::string &ip, const std::string &pdata)
		: type(request_type), document(uri), ipaddr(ip), postdata(pdata), headers(hdr), sock(opaque)
	{
	}

	/** Get headers.
	 * All the headers for the web request are returned, as a pointer to a stringstream.
	 * @return The header information
	 */
	std::stringstream* GetHeaders()
	{
		return headers;
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
	std::string extraheaders;

 public:

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
	HTTPDocument(void* opaque, std::stringstream* doc, int response, const std::string &extra) : document(doc), responsecode(response), extraheaders(extra), sock(opaque)
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

	/** Get the headers.
	 * @return The header text, headers seperated by carriage return and linefeed.
	 */
	std::string& GetExtraHeaders()
	{
		return this->extraheaders;
	}
};

#endif

