/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2012 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef __TRANSPORT_H__
#define __TRANSPORT_H__

#include <map>
#include <string>

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
 public:
	std::string dn;
	std::string issuer;
	std::string error;
	std::string fingerprint;
	bool trusted, invalid, unknownsigner, revoked;

	/** Get certificate distinguished name
	 * @return Certificate DN
	 */
	const std::string& GetDN()
	{
		return dn;
	}

	/** Get Certificate issuer
	 * @return Certificate issuer
	 */
	const std::string& GetIssuer()
	{
		return issuer;
	}

	/** Get error string if an error has occured
	 * @return The error associated with this users certificate,
	 * or an empty string if there is no error.
	 */
	const std::string& GetError()
	{
		return error;
	}

	/** Get key fingerprint.
	 * @return The key fingerprint as a hex string.
	 */
	const std::string& GetFingerprint()
	{
		return fingerprint;
	}

	/** Get trust status
	 * @return True if this is a trusted certificate
	 * (the certificate chain validates)
	 */
	bool IsTrusted()
	{
		return trusted;
	}

	/** Get validity status
	 * @return True if the certificate itself is
	 * correctly formed.
	 */
	bool IsInvalid()
	{
		return invalid;
	}

	/** Get signer status
	 * @return True if the certificate appears to be
	 * self-signed.
	 */
	bool IsUnknownSigner()
	{
		return unknownsigner;
	}

	/** Get revokation status.
	 * @return True if the certificate is revoked.
	 * Note that this only works properly for GnuTLS
	 * right now.
	 */
	bool IsRevoked()
	{
		return revoked;
	}

	std::string GetMetaLine()
	{
		std::stringstream value;
		bool hasError = error.length();
		value << (IsInvalid() ? "v" : "V") << (IsTrusted() ? "T" : "t") << (IsRevoked() ? "R" : "r")
			<< (IsUnknownSigner() ? "s" : "S") << (hasError ? "E" : "e") << " ";
		if (hasError)
			value << GetError();
		else
			value << GetFingerprint() << " " << GetDN() << " " << GetIssuer();
		return value.str();
	}
};

/** Used to represent a request to a transport provider module
 */
class ISHRequest : public Request
{
 public:
	BufferedSocket* Sock;

	ISHRequest(Module* Me, Module* Target, const char* rtype, BufferedSocket* sock) : Request(Me, Target, rtype), Sock(sock)
	{
	}
};

/** Used to represent a request to attach a cert to an BufferedSocket
 */
class BufferedSocketAttachCertRequest : public ISHRequest
{
 public:
	/** Initialize the request as an attach cert message */
	BufferedSocketAttachCertRequest(BufferedSocket* is, Module* Me, Module* Target) : ISHRequest(Me, Target, "IS_ATTACH", is)
	{
	}
};

/** Used to check if a handshake is complete on an BufferedSocket yet
 */
class BufferedSocketHSCompleteRequest : public ISHRequest
{
 public:
	/** Initialize the request as a 'handshake complete?' message */
	BufferedSocketHSCompleteRequest(BufferedSocket* is, Module* Me, Module* Target) : ISHRequest(Me, Target, "IS_HSDONE", is)
	{
	}
};

/** Used to hook a transport provider to an BufferedSocket
 */
class BufferedSocketHookRequest : public ISHRequest
{
 public:
	/** Initialize request as a hook message */
	BufferedSocketHookRequest(BufferedSocket* is, Module* Me, Module* Target) : ISHRequest(Me, Target, "IS_HOOK", is)
	{
	}
};

/** Used to unhook a transport provider from an BufferedSocket
 */
class BufferedSocketUnhookRequest : public ISHRequest
{
 public:
	/** Initialize request as an unhook message */
	BufferedSocketUnhookRequest(BufferedSocket* is, Module* Me, Module* Target) : ISHRequest(Me, Target, "IS_UNHOOK", is)
	{
	}
};

class BufferedSocketNameRequest : public ISHRequest
{
 public:
	/** Initialize request as a get name message */
	BufferedSocketNameRequest(Module* Me, Module* Target) : ISHRequest(Me, Target, "IS_NAME", NULL)
	{
	}
};

class BufferedSocketFingerprintRequest : public ISHRequest
{
 public:
	/** Initialize request as a fingerprint message */
	BufferedSocketFingerprintRequest(BufferedSocket* is, Module* Me, Module* Target) : ISHRequest(Me, Target, "GET_FP", is)
	{
	}
};

#endif
