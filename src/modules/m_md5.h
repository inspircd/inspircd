#ifndef __MD5_H__
#define __MD5_H__

#include "modules.h"

class MD5Request : public Request
{
	unsigned int* keys;
	const char* outputs;
	std::string tohash;
 public:
	MD5Request(Module* Me, Module* Target) : Request(Me, Target, "MD5_RESET")
	{
	}

	MD5Request(Module* Me, Module* Target, const std::string &hashable) : Request(Me, Target, "MD5_SUM"), keys(NULL), outputs(NULL), tohash(hashable)
	{
	}

	MD5Request(Module* Me, Module* Target, unsigned int* k) : Request(Me, Target, "MD5_KEY"), keys(k), outputs(NULL), tohash("")
	{
	}

	MD5Request(Module* Me, Module* Target, const char* out) : Request(Me, Target, "MD5_HEX"), keys(NULL), outputs(out), tohash("")
	{
	}

	const char* GetHashData()
	{
		return tohash.c_str();
	}

	unsigned int* GetKeyData()
	{
		return keys;
	}

	const char* GetOutputs()
	{
		return outputs;
	}
};

class MD5ResetRequest : public MD5Request
{
 public:
	MD5ResetRequest(Module* Me, Module* Target) : MD5Request(Me, Target)
	{
	}
};

class MD5SumRequest : public MD5Request
{
 public:
	MD5SumRequest(Module* Me, Module* Target, const std::string &data) : MD5Request(Me, Target, data)
	{
	}
};

class MD5KeyRequest : public MD5Request
{
	MD5KeyRequest(Module* Me, Module* Target, unsigned int* data) : MD5Request(Me, Target, data)
	{
	}
};

class MD5HexRequest : public MD5Request
{
	MD5HexRequest(Module* Me, Module* Target, const char* data) : MD5Request(Me, Target, data)
	{
	}
};

#endif

