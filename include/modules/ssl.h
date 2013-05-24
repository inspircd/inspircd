/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
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


#pragma once

#include <map>
#include <string>
#include "iohook.h"

/** ssl_cert is a class which abstracts SSL certificate
 * and key information.
 *
 * Because gnutls and openssl represent key information in
 * wildly different ways, this class allows it to be accessed
 * in a unified manner. These classes are attached to ssl-
 * connected local users using SSLCertExt
 */
class ssl_cert : public refcountbase
{
 public:
	std::string dn;
	std::string issuer;
	std::string error;
	std::string fingerprint;
	bool trusted, invalid, unknownsigner, revoked;

	ssl_cert() : trusted(false), invalid(true), unknownsigner(true), revoked(false) {}

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

	bool IsCAVerified()
	{
		return trusted && !invalid && !revoked && !unknownsigner && error.empty();
	}

	std::string GetMetaLine()
	{
		std::stringstream value;
		bool hasError = !error.empty();
		value << (IsInvalid() ? "v" : "V") << (IsTrusted() ? "T" : "t") << (IsRevoked() ? "R" : "r")
			<< (IsUnknownSigner() ? "s" : "S") << (hasError ? "E" : "e") << " ";
		if (hasError)
			value << GetError();
		else
			value << GetFingerprint() << " " << GetDN() << " " << GetIssuer();
		return value.str();
	}
};

class SSLIOHook : public IOHook
{
 public:
	SSLIOHook(Module* mod, const std::string& Name)
		: IOHook(mod, Name, IOHook::IOH_SSL)
	{
	}

	/**
	 * Get the client certificate from a socket
	 * @param sock The socket to get the certificate from, must be using this IOHook
	 * @return The SSL client certificate information
	 */
	virtual ssl_cert* GetCertificate(StreamSocket* sock) = 0;

	/**
	 * Get the fingerprint of a client certificate from a socket
	 * @param sock The socket to get the certificate fingerprint from, must be using this IOHook
	 * @return The fingerprint of the SSL client certificate sent by the peer,
	 * empty if no cert was sent
	 */
	std::string GetFingerprint(StreamSocket* sock)
	{
		ssl_cert* cert = GetCertificate(sock);
		if (cert)
			return cert->GetFingerprint();
		return "";
	}
};

/** Helper functions for obtaining SSL client certificates and key fingerprints
 * from StreamSockets
 */
class SSLClientCert
{
 public:
 	/**
	 * Get the client certificate from a socket
	 * @param sock The socket to get the certificate from, the socket does not have to use SSL
	 * @return The SSL client certificate information, NULL if the peer is not using SSL
	 */
	static ssl_cert* GetCertificate(StreamSocket* sock)
	{
		IOHook* iohook = sock->GetIOHook();
		if ((!iohook) || (iohook->type != IOHook::IOH_SSL))
			return NULL;

		SSLIOHook* ssliohook = static_cast<SSLIOHook*>(iohook);
		return ssliohook->GetCertificate(sock);
	}

	/**
	 * Get the fingerprint of a client certificate from a socket
	 * @param sock The socket to get the certificate fingerprint from, the
	 * socket does not have to use SSL
	 * @return The key fingerprint from the SSL certificate sent by the peer,
	 * empty if no cert was sent or the peer is not using SSL
	 */
	static std::string GetFingerprint(StreamSocket* sock)
	{
		ssl_cert* cert = SSLClientCert::GetCertificate(sock);
		if (cert)
			return cert->GetFingerprint();
		return "";
	}
};

/** Get certificate from a user (requires m_sslinfo) */
struct UserCertificateRequest : public Request
{
	User* const user;
	ssl_cert* cert;

	UserCertificateRequest(User* u, Module* Me, Module* info = ServerInstance->Modules->Find("m_sslinfo.so"))
		: Request(Me, info, "GET_USER_CERT"), user(u), cert(NULL)
	{
		Send();
	}

	std::string GetFingerprint()
	{
		if (cert)
			return cert->GetFingerprint();
		return "";
	}
};
