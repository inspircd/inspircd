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

#include "inspircd.h"
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "httpd.h"
#include "rpc.h"
#include <exception>

/* $ModDesc: Encode and decode JSON-RPC requests for modules */
/* $ModDep: httpd.h rpc.h */

class JsonException : public std::exception
{
 private:
	std::string _what;
 public:
	JsonException(const std::string &what)
		: _what(what)
	{
	}
	
	virtual ~JsonException() throw() { }
	
	virtual const char *what() const throw()
	{
		return _what.c_str();
	}
};

class ModuleRpcJson : public Module
{
 private:
	
 public:
	ModuleRpcJson(InspIRCd *Me) : Module(Me)
	{
		ServerInstance->Modules->PublishInterface("RPC", this);
		Implementation eventlist[] = { I_OnEvent };
		ServerInstance->Modules->Attach(eventlist, this, 1);
	}
	
	virtual ~ModuleRpcJson()
	{
		ServerInstance->Modules->UnpublishInterface("RPC", this);
	}
	
	virtual Version GetVersion()
	{
		return Version(1, 1, 0, 0, VF_SERVICEPROVIDER | VF_VENDOR, API_VERSION);
	}
	
	
	virtual void OnEvent(Event *event)
	{
		if (event->GetEventID() == "httpd_url")
		{
			HTTPRequest *req = (HTTPRequest*) event->GetData();
			
			if ((req->GetURI() == "/rpc/json") || (req->GetURI() == "/rpc/json/"))
			{
				std::stringstream data;
				
				RPCValue *reqobj = NULL;
				
				try
				{
					reqobj = this->JSONParse(req->GetPostData());
					
					if (!reqobj || (reqobj->GetType() != RPCObject))
						throw JsonException("RPC requests must be in the form of a single object");
					
					RPCValue *method = reqobj->GetObject("method");
					if (!method || method->GetType() != RPCString)
						throw JsonException("RPC requests must have a 'method' string field");
					
					RPCValue *params = reqobj->GetObject("params");
					if (!params || params->GetType() != RPCArray)
						throw JsonException("RPC requests must have a 'params' array field");
					
					RPCRequest modreq("json", method->GetString(), params);
					Event mev((char*) &modreq, this, "RPCMethod");
					mev.Send(ServerInstance);
					
					if (!modreq.claimed)
						throw JsonException("Unrecognized method");
					
					if (!modreq.error.empty())
					{
						data << "{\"result\":null,\"error\":\"" << modreq.error << "\"";
					}
					else
					{
						data << "{\"result\":";
						this->JSONSerialize(modreq.result, data);
						data << ",\"error\":null";
					}
					
					if (reqobj->GetObject("id"))
					{
						data << ",\"id\":";
						this->JSONSerialize(reqobj->GetObject("id"), data);
					}
					data << "}";
										
					delete reqobj;
					reqobj = NULL;
				}
				catch (std::exception &e)
				{
					if (reqobj)
						delete reqobj;
					data << "{\"result\":null,\"error\":\"" << e.what() << "\"}";
				}
				
				HTTPDocument response(req->sock, &data, 200);
				response.headers.SetHeader("X-Powered-By", "m_rpc_json.so");
				response.headers.SetHeader("Content-Type", "application/json");
				response.headers.SetHeader("Connection", "Keep-Alive");
				
				Request rreq((char*) &response, (Module*) this, event->GetSource());
				rreq.Send();
			}
		}
	}
	
	void AttachToParent(RPCValue *parent, RPCValue *child, const std::string &key = "")
	{
		if (!parent || !child)
			return;
		
		if (parent->GetType() == RPCArray)
			parent->ArrayAdd(child);
		else if (parent->GetType() == RPCObject)
			parent->ObjectAdd(key, child);
		else
			throw JsonException("Cannot add a value to a non-container");
	}
	
	void AttachToParentReset(RPCValue *parent, RPCValue *&child, std::string &key)
	{
		AttachToParent(parent, child, key);
		child = NULL;
		key.clear();
	}
	
	RPCValue *JSONParse(const std::string &data)
	{
		bool pisobject = false;
		bool instring = false;
		std::string stmp;
		std::string vkey;
		std::string pvkey;
		RPCValue *aparent = NULL;
		RPCValue *value = NULL;
		
		for (std::string::const_iterator i = data.begin(); i != data.end(); i++)
		{
			if (instring)
			{
				// TODO escape sequences
				if (*i == '"')
				{
					instring = false;
					
					if (pisobject && vkey.empty())
						vkey = stmp;
					else
						value = new RPCValue(stmp);
					
					stmp.clear();
				}
				else
					stmp += *i;
				
				continue;
			}
			
			if ((*i == ' ') || (*i == '\t') || (*i == '\r') || (*i == '\n'))
				continue;
			
			if (*i == '{')
			{
				// Begin object
				if ((value) || (pisobject && vkey.empty()))
					throw JsonException("Unexpected begin object token ('{')");
				
				RPCValue *nobj = new RPCValue(RPCObject, aparent);
				aparent = nobj;
				pvkey = vkey;
				vkey.clear();
				pisobject = true;
			}
			else if (*i == '}')
			{
				// End object
				if ((!aparent) || (!pisobject) || (!vkey.empty() && !value))
					throw JsonException("Unexpected end object token ('}')");
				
				// End value
				if (value)
					AttachToParentReset(aparent, value, vkey);
				
				if (!aparent->parent)
					return aparent;
				
				value = aparent;
				aparent = aparent->parent;
				vkey = pvkey;
				pvkey.clear();
				pisobject = (aparent->GetType() == RPCObject);
			}
			else if (*i == '"')
			{
				// Begin string
				if (value)
					throw JsonException("Unexpected begin string token ('\"')");
				
				instring = true;
			}
			else if (*i == ':')
			{
				if ((!aparent) || (!pisobject) || (vkey.empty()) || (value))
					throw JsonException("Unexpected object value token (':')");
			}
			else if (*i == ',')
			{
				if ((!aparent) || (!value) || ((pisobject) && (vkey.empty())))
					throw JsonException("Unexpected value seperator token (',')");
				
				AttachToParentReset(aparent, value, vkey);
			}
			else if (*i == '[')
			{
				// Begin array
				if ((value) || (pisobject && vkey.empty()))
					throw JsonException("Unexpected begin array token ('[')");
				
				RPCValue *nar = new RPCValue(RPCArray, aparent);
				aparent = nar;
				pvkey = vkey;
				vkey.clear();
				pisobject = false;
			}
			else if (*i == ']')
			{
				// End array (also an end value delimiter)
				if (!aparent || pisobject)
					throw JsonException("Unexpected end array token (']')");
				
				if (value)
					AttachToParentReset(aparent, value, vkey);
				
				if (!aparent->parent)
					return aparent;
				
				value = aparent;
				aparent = aparent->parent;
				vkey = pvkey;
				pvkey.clear();
				pisobject = (aparent->GetType() == RPCObject);
			}
			else
			{
				// Numbers, false, null, and true fall under this heading.
				if ((*i == 't') && ((i + 3) < data.end()) && (*(i + 1) == 'r') && (*(i + 2) == 'u') && (*(i + 3) == 'e'))
				{
					value = new RPCValue(true);
					i += 3;
				}
				else if ((*i == 'f') && ((i + 4) < data.end()) && (*(i + 1) == 'a') && (*(i + 2) == 'l') && (*(i + 3) == 's') && (*(i + 4) == 'e'))
				{
					value = new RPCValue(false);
					i += 4;
				}
				else if ((*i == 'n') && ((i + 3) < data.end()) && (*(i + 1) == 'u') && (*(i + 2) == 'l') && (*(i + 3) == 'l'))
				{
					value = new RPCValue();
					i += 3;
				}
				else if ((*i == '-') || (*i == '+') || (*i == '.') || ((*i >= '0') && (*i <= '9')))
				{
					std::string ds = std::string(i, data.end());
					char *eds = NULL;
					
					errno = 0;
					double v = strtod(ds.c_str(), &eds);
					
					if (errno != 0)
						throw JsonException("Error parsing numeric value");
					
					value = new RPCValue(v);
					
					i += eds - ds.c_str() - 1;
				}
				else
					throw JsonException("Unknown data in value portion");
			}
		}
		
		if (instring)
			throw JsonException("Unterminated string");
		
		if (aparent && pisobject)
			throw JsonException("Unterminated object");
		else if (aparent && !pisobject)
			throw JsonException("Unterminated array");
		
		if (value)
			return value;
		else
			throw JsonException("No JSON data found");
	}
	
	void JSONSerialize(RPCValue *value, std::stringstream &re)
	{
		int ac;
		switch (value->GetType())
		{
			case RPCNull:
				re << "null";
				break;
			case RPCBoolean:
				re << ((value->GetBool()) ? "true" : "false");
				break;
			case RPCInteger:
				re << value->GetInt();
				break;
			case RPCString:
				re << "\"" << value->GetString() << "\"";
				break;
			case RPCArray:
				re << "[";
				ac = value->ArraySize();
				for (int i = 0; i < ac; i++)
				{
					this->JSONSerialize(value->GetArray(i), re);
					if (i != (ac - 1))
						re << ",";
				}
				re << "]";
				break;
			case RPCObject:
				re << "{";
				std::pair<RPCObjectContainer::iterator,RPCObjectContainer::iterator> its = value->GetObjectIterator();
				while (its.first != its.second)
				{
					re << "\"" << its.first->first << "\":";
					this->JSONSerialize(its.first->second, re);
					if (++its.first != its.second)
						re << ",";
				}
				re << "}";
				break;
		}
	}
};

MODULE_INIT(ModuleRpcJson)
