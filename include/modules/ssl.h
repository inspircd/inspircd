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
	bool trusted, invalid, unknownsigner, revoked, exists;

	ssl_cert() : trusted(false), invalid(true), unknownsigner(true), revoked(false), exists(false) {}

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

	/** Get certificate usability
	* @return True if the certificate is not expired nor revoked
	*/
	bool IsUsable()
	{
		return !invalid && !revoked && error.empty();
	}

	/** Get CA trust status
	* @return True if the certificate is issued by a CA
	* and valid.
	*/
	bool IsCAVerified()
	{
		return IsUsable() && trusted && !unknownsigner;
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
 protected:
	/** Peer SSL certificate, set by the SSL module
	 */
	reference<ssl_cert> certificate;
	std::vector<reference<ssl_cert>> chain;

	/** Reduce elements in a send queue by appending later elements to the first element until there are no more
	 * elements to append or a desired length is reached
	 * @param sendq SendQ to work on
	 * @param targetsize Target size of the front element
	 */
	static void FlattenSendQueue(StreamSocket::SendQueue& sendq, size_t targetsize)
	{
		if ((sendq.size() <= 1) || (sendq.front().length() >= targetsize))
			return;

		// Avoid multiple repeated SSL encryption invocations
		// This adds a single copy of the queue, but avoids
		// much more overhead in terms of system calls invoked
		// by an IOHook.
		std::string tmp;
		tmp.reserve(std::min(targetsize, sendq.bytes())+1);
		do
		{
			tmp.append(sendq.front());
			sendq.pop_front();
		}
		while (!sendq.empty() && tmp.length() < targetsize);
		sendq.push_front(tmp);
	}

 public:
	static SSLIOHook* IsSSL(StreamSocket* sock)
	{
		IOHook* const iohook = sock->GetIOHook();
		if ((iohook) && ((iohook->prov->type == IOHookProvider::IOH_SSL)))
			return static_cast<SSLIOHook*>(iohook);
		return NULL;
	}

	SSLIOHook(IOHookProvider* hookprov)
		: IOHook(hookprov)
	{
	}

	/**
	 * Get the certificate sent by this peer
	 * @return The SSL certificate sent by the peer, NULL if no cert was sent
	 */
	ssl_cert* GetCertificate() const
	{
		if (certificate && certificate->IsUsable())
			return certificate;
		return NULL;
	}

	/**
	 * Get the fingerprint of the peer's certificate
	 * @return The fingerprint of the SSL client certificate sent by the peer,
	 * empty if no cert was sent
	 */
	std::string GetFingerprint() const
	{
		ssl_cert* cert = GetCertificate();
		if (cert)
			return cert->GetFingerprint();
		return "";
	}

	/**
	 * Get the ciphersuite negotiated with the peer
	 * @param out String where the ciphersuite string will be appended to
	 */
	virtual void GetCiphersuite(std::string& out) const = 0;


	/** Retrieves the name of the SSL connection which is sent via SNI.
	 * @param out String that the server name will be appended to.
	 * returns True if the server name was retrieved; otherwise, false.
	 */
	virtual bool GetServerName(std::string& out) const = 0;
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
		SSLIOHook* ssliohook = SSLIOHook::IsSSL(sock);
		if (!ssliohook)
			return NULL;

		return ssliohook->GetCertificate();
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

class UserCertificateAPIBase : public DataProvider
{
 public:
 	UserCertificateAPIBase(Module* parent)
		: DataProvider(parent, "m_sslinfo_api")
	{
	}

	/** Get the SSL certificate of a user
	 * @param user The user whose certificate to get, user may be remote
	 * @return The SSL certificate of the user or NULL if the user is not using SSL
	 */
	virtual ssl_cert* GetCertificate(User* user) = 0;

	/** Get the key fingerprint from a user's certificate
	 * @param user The user whose key fingerprint to get, user may be remote
	 * @return The key fingerprint from the user's SSL certificate or an empty string
	 * if the user is not using SSL or did not provide a client certificate
	 */
	std::string GetFingerprint(User* user)
	{
		ssl_cert* cert = GetCertificate(user);
		if (cert)
			return cert->GetFingerprint();
		return "";
	}
};

/** API implemented by m_sslinfo that allows modules to retrive the SSL certificate
 * information of local and remote users. It can also be used to find out whether a
 * user is using SSL or not.
 */
class UserCertificateAPI : public dynamic_reference<UserCertificateAPIBase>
{
 public:
	UserCertificateAPI(Module* parent)
		: dynamic_reference<UserCertificateAPIBase>(parent, "m_sslinfo_api")
	{
	}
};
