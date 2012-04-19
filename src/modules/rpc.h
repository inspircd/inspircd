/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2012 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef RPC_H
#define RPC_H

#include <string>
#include <map>
#include <stdexcept>

class RPCValue;

typedef enum
{
	RPCNull,
	RPCBoolean,
	RPCInteger,
	RPCString,
	RPCArray,
	RPCObject
} RPCValueType;

typedef std::map<std::string,RPCValue*> RPCObjectContainer;
typedef std::vector<RPCValue*> RPCArrayContainer;

class RPCValue : public classbase
{
 protected:
	RPCValueType type;
	void *value;

	double *CastInteger()
	{
		return (double*)value;
	}

	std::string *CastString()
	{
		return (std::string*)value;
	}

	RPCObjectContainer *CastObject()
	{
		return (RPCObjectContainer*)value;
	}

	RPCArrayContainer *CastArray()
	{
		return (RPCArrayContainer*)value;
	}

	void DestroyValue()
	{
		// Some versions of GCC complain about declaration in switch statements
		RPCArrayContainer *a;
		RPCObjectContainer *o;
		switch (type)
		{
			case RPCInteger:
				delete this->CastInteger();
				break;
			case RPCString:
				delete this->CastString();
				break;
			case RPCArray:
				a = this->CastArray();
				for (RPCArrayContainer::iterator i = a->begin(); i != a->end(); i++)
					delete *i;
				delete a;
				break;
			case RPCObject:
				o = this->CastObject();
				for (RPCObjectContainer::iterator i = o->begin(); i != o->end(); i++)
					delete i->second;
				delete o;
				break;
			default:
				break;
		}

		value = NULL;
	}

	void InitValue()
	{
		switch (type)
		{
			case RPCNull:
			case RPCBoolean:
				value = NULL;
				break;
			case RPCInteger:
				value = new double;
				break;
			case RPCString:
				value = new std::string;
				break;
			case RPCArray:
				value = new RPCArrayContainer;
				break;
			case RPCObject:
				value = new RPCObjectContainer;
				break;
		}
	}

	RPCValue(const RPCValue &v) { }

 public:
	RPCValue *parent;

	RPCValue(RPCValue *rparent = NULL) : type(RPCNull), value(NULL), parent(rparent) { }
	RPCValue(RPCValueType ttype, RPCValue *rparent = NULL) : type(ttype), value(NULL), parent(rparent) { InitValue(); }
	RPCValue(bool nvalue, RPCValue *rparent = NULL) : type(RPCBoolean), value((void*)nvalue), parent(rparent) { }
	RPCValue(double nvalue, RPCValue *rparent = NULL) : type(RPCInteger), parent(rparent) { value = new double(nvalue); }
	RPCValue(const std::string &nvalue, RPCValue *rparent = NULL) : type(RPCString), parent(rparent) { value = new std::string(nvalue); }

	virtual ~RPCValue()
	{
		DestroyValue();
	}

	RPCValueType GetType()
	{
		return type;
	}

	void SetNull()
	{
		DestroyValue();
		type = RPCNull;
	}

	void SetBoolean(bool nvalue)
	{
		DestroyValue();
		value = (void*)nvalue;
		type = RPCBoolean;
	}

	void SetInteger(double nvalue)
	{
		if (type == RPCInteger)
		{
			*this->CastInteger() = nvalue;
		}
		else
		{
			DestroyValue();
			value = new double(nvalue);
			type = RPCInteger;
		}
	}

	void SetString(const std::string &nvalue)
	{
		if (type == RPCString)
		{
			this->CastString()->assign(nvalue);
		}
		else
		{
			DestroyValue();
			value = new std::string(nvalue);
			type = RPCString;
		}
	}

	void SetArray()
	{
		if (type == RPCArray)
		{
			this->CastArray()->clear();
		}
		else
		{
			DestroyValue();
			type = RPCArray;
			InitValue();
		}
	}

	void SetObject()
	{
		if (type == RPCObject)
		{
			this->CastObject()->clear();
		}
		else
		{
			DestroyValue();
			type = RPCObject;
			InitValue();
		}
	}

	void ArrayAdd(RPCValue *nvalue)
	{
		if (type != RPCArray)
			return;
		RPCArrayContainer *a = this->CastArray();
		a->push_back(nvalue);
		nvalue->parent = this;
	}

	void ObjectAdd(const std::string &key, RPCValue *nvalue)
	{
		if (type != RPCObject)
			return;
		RPCObjectContainer *o = this->CastObject();
		o->insert(std::make_pair(key, nvalue));
		nvalue->parent = this;
	}

	RPCValue *GetArray(int i)
	{
		if (type != RPCArray)
			return NULL;
		RPCArrayContainer *a = this->CastArray();
		if ((i < 0) || (i >= (signed)a->size()))
			return NULL;
		return a->at(i);
	}

	int ArraySize()
	{
		if (type != RPCArray)
			return 0;
		RPCArrayContainer *a = this->CastArray();
		return a->size();
	}

	RPCValue *GetObject(const std::string &key)
	{
		if (type != RPCObject)
			return NULL;
		RPCObjectContainer *o = this->CastObject();
		RPCObjectContainer::iterator it = o->find(key);
		if (it == o->end())
			return NULL;
		return it->second;
	}

	std::pair<RPCObjectContainer::iterator,RPCObjectContainer::iterator> GetObjectIterator()
	{
		if (type != RPCObject)
			throw std::runtime_error("Cannot get iterator for a non-object RPC value");
		RPCObjectContainer *o = this->CastObject();
		return std::make_pair(o->begin(), o->end());
	}

	std::string GetString()
	{
		if (type != RPCString)
			return std::string();
		return *this->CastString();
	}

	double GetInt()
	{
		if (type != RPCInteger)
			return 0;
		return *this->CastInteger();
	}

	bool GetBool()
	{
		if (type != RPCBoolean)
			return 0;
		return (value != NULL);
	}
};

class RPCRequest : public classbase
{
 protected:

 public:
	std::string method;
	RPCValue *parameters;
	RPCValue *result;
	std::string provider;
	bool claimed;
	std::string error;

	RPCRequest(const std::string &sprovider, const std::string &smethod, RPCValue *rparameters)
		: method(smethod), parameters(rparameters), provider(sprovider), claimed(false)
	{
		result = new RPCValue();
	}

	~RPCRequest()
	{
		if (result)
			delete result;
	}
};

#endif
