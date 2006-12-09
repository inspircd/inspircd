#ifndef __SSL_CERT_H__
#define __SSL_CERT_H__

#include <map>
#include <string>

/** A generic container for certificate data
 */
typedef std::map<std::string,std::string> ssl_data;

/** A shorthand way of representing an iterator into ssl_data
 */
typedef ssl_data::iterator ssl_data_iter;

/** ssl_cert is a class which abstracts SSL certificate
 * and key information.
 *
 * Because gnutls and openssl represent key information in
 * wildly different ways, this class allows it to be accessed
 * in a unified manner. These classes are attached to ssl-
 * connected local users using Extensible::Extend() and the
 * key 'ssl_cert'.
 */
class ssl_cert
{
	/** Always contains an empty string
	 */
	const std::string empty;

 public:
	/** The data for this certificate
	 */
	ssl_data data;

	/** Default constructor, initializes 'empty'
	 */
	ssl_cert() : empty("")
	{
	}
	
	/** Get certificate distinguished name
	 * @return Certificate DN
	 */
	const std::string& GetDN()
	{
		ssl_data_iter ssldi = data.find("dn");

		if (ssldi != data.end())
			return ssldi->second;
		else
			return empty;
	}

	/** Get Certificate issuer
	 * @return Certificate issuer
	 */
	const std::string& GetIssuer()
	{
		ssl_data_iter ssldi = data.find("issuer");

		if (ssldi != data.end())
			return ssldi->second;
		else
			return empty;
	}

	/** Get error string if an error has occured
	 * @return The error associated with this users certificate,
	 * or an empty string if there is no error.
	 */
	const std::string& GetError()
	{
		ssl_data_iter ssldi = data.find("error");

		if (ssldi != data.end())
			return ssldi->second;
		else
			return empty;
	}

	/** Get key fingerprint.
	 * @return The key fingerprint as a hex string.
	 */
	const std::string& GetFingerprint()
	{
		ssl_data_iter ssldi = data.find("fingerprint");

		if (ssldi != data.end())
			return ssldi->second;
		else
			return empty;
	}

	/** Get trust status
	 * @return True if this is a trusted certificate
	 * (the certificate chain validates)
	 */
	bool IsTrusted()
	{
		ssl_data_iter ssldi = data.find("trusted");

		if (ssldi != data.end())
			return (ssldi->second == "1");
		else
			return false;
	}

	/** Get validity status
	 * @return True if the certificate itself is
	 * correctly formed.
	 */
	bool IsInvalid()
	{
		ssl_data_iter ssldi = data.find("invalid");

		if (ssldi != data.end())
			return (ssldi->second == "1");
		else
			return false;
	}

	/** Get signer status
	 * @return True if the certificate appears to be
	 * self-signed.
	 */
	bool IsUnknownSigner()
	{
		ssl_data_iter ssldi = data.find("unknownsigner");

		if (ssldi != data.end())
			return (ssldi->second == "1");
		else
			return false;
	}

	/** Get revokation status.
	 * @return True if the certificate is revoked.
	 * Note that this only works properly for GnuTLS
	 * right now.
	 */
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

