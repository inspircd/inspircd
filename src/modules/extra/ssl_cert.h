#ifndef __SSL_CERT_H__
#define __SSL_CERT_H__

#include <map>
#include <string>

typedef std::map<std::string,std::string> ssl_data;
typedef ssl_data::iterator ssl_data_iter;

class ssl_cert
{
	const std::string empty;

 public:
	ssl_data data;

	ssl_cert() : empty("")
	{
	}

	const std::string& GetDN()
	{
		ssl_data_iter ssldi = data.find("dn");

		if (ssldi != data.end())
			return ssldi->second;
		else
			return empty;
	}

	const std::string& GetIssuer()
	{
		ssl_data_iter ssldi = data.find("issuer");

		if (ssldi != data.end())
			return ssldi->second;
		else
			return empty;
	}

	const std::string& GetError()
	{
		ssl_data_iter ssldi = data.find("error");

		if (ssldi != data.end())
			return ssldi->second;
		else
			return empty;
	}

	bool IsTrusted()
	{
		ssl_data_iter ssldi = data.find("trusted");

		if (ssldi != data.end())
			return (ssldi->second == "1");
		else
			return false;
	}

	bool IsInvalid()
	{
		ssl_data_iter ssldi = data.find("invalid");

		if (ssldi != data.end())
			return (ssldi->second == "1");
		else
			return false;
	}

	bool IsUnknownSigner()
	{
		ssl_data_iter ssldi = data.find("unknownsigner");

		if (ssldi != data.end())
			return (ssldi->second == "1");
		else
			return false;
	}

	bool IsRevoked()
	{
		ssl_data_iter ssldi = data.find("revoked");

		if (ssldi != data.end())
			return (ssldi->second == "1");
		else
			return false;
	}
};

#endif

