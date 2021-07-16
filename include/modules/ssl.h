/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2020 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2019 B00mX0r <b00mx0r@aureus.pw>
 *   Copyright (C) 2018 Dylan Frank <b00mx0r@aureus.pw>
 *   Copyright (C) 2013, 2017-2019, 2021 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013, 2015-2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2012 ChrisTX <xpipe@hotmail.de>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006 Craig Edwards <brain@inspircd.org>
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

/** ssl_cert is a class which abstracts TLS certificate
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
	bool trusted = false;
	bool invalid = true;
	bool unknownsigner = true;
	bool revoked = false;

	/** Get certificate distinguished name
	 * @return Certificate DN
	 */
	const std::string& GetDN() const
	{
		return dn;
	}

	/** Get Certificate issuer
	 * @return Certificate issuer
	 */
	const std::string& GetIssuer() const
	{
		return issuer;
	}

	/** Get error string if an error has occurred
	 * @return The error associated with this users certificate,
	 * or an empty string if there is no error.
	 */
	const std::string& GetError() const
	{
		return error;
	}

	/** Get key fingerprint.
	 * @return The key fingerprint as a hex string.
	 */
	const std::string& GetFingerprint() const
	{
		return fingerprint;
	}

	/** Get trust status
	 * @return True if this is a trusted certificate
	 * (the certificate chain validates)
	 */
	bool IsTrusted() const
	{
		return trusted;
	}

	/** Get validity status
	 * @return True if the certificate itself is
	 * correctly formed.
	 */
	bool IsInvalid() const
	{
		return invalid;
	}

	/** Get signer status
	 * @return True if the certificate appears to be
	 * self-signed.
	 */
	bool IsUnknownSigner() const
	{
		return unknownsigner;
	}

	/** Get revocation status.
	 * @return True if the certificate is revoked.
	 * Note that this only works properly for GnuTLS
	 * right now.
	 */
	bool IsRevoked() const
	{
		return revoked;
	}

	/** Get certificate usability
	* @return True if the certificate is not expired nor revoked
	*/
	bool IsUsable() const
	{
		return !invalid && !revoked && error.empty();
	}

	/** Get CA trust status
	* @return True if the certificate is issued by a CA
	* and valid.
	*/
	bool IsCAVerified() const
	{
		return IsUsable() && trusted && !unknownsigner;
	}

};

/** I/O hook provider for TLS modules. */
class SSLIOHookProvider : public IOHookProvider
{
public:
	SSLIOHookProvider(Module* mod, const std::string& Name)
		: IOHookProvider(mod, "ssl/" + Name, IOH_SSL)
	{
	}
};

class SSLIOHook : public IOHook
{
 protected:
	/** Peer TLS certificate, set by the TLS module
	 */
	reference<ssl_cert> certificate;

	/** Reduce elements in a send queue by appending later elements to the first element until there are no more
	 * elements to append or a desired length is reached
	 * @param sendq SendQ to work on
	 * @param targetsize Target size of the front element
	 */
	static void FlattenSendQueue(StreamSocket::SendQueue& sendq, size_t targetsize)
	{
		if ((sendq.size() <= 1) || (sendq.front().length() >= targetsize))
			return;

		// Avoid multiple repeated TLS encryption invocations
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
		IOHook* const lasthook = sock->GetLastHook();
		if (lasthook && (lasthook->prov->type == IOHookProvider::IOH_SSL))
			return static_cast<SSLIOHook*>(lasthook);

		return NULL;
	}

	SSLIOHook(std::shared_ptr<IOHookProvider> hookprov)
		: IOHook(hookprov)
	{
	}

	/**
	 * Get the certificate sent by this peer
	 * @return The TLS certificate sent by the peer, NULL if no cert was sent
	 */
	virtual ssl_cert* GetCertificate() const
	{
		return certificate;
	}

	/**
	 * Get the fingerprint of the peer's certificate
	 * @return The fingerprint of the TLS client certificate sent by the peer,
	 * empty if no cert was sent
	 */
	virtual std::string GetFingerprint() const
	{
		ssl_cert* cert = GetCertificate();
		if (cert && cert->IsUsable())
			return cert->GetFingerprint();
		return "";
	}

	/**
	 * Get the ciphersuite negotiated with the peer
	 * @param out String where the ciphersuite string will be appended to
	 */
	virtual void GetCiphersuite(std::string& out) const = 0;


	/** Retrieves the name of the TLS connection which is sent via SNI.
	 * @param out String that the server name will be appended to.
	 * returns True if the server name was retrieved; otherwise, false.
	 */
	virtual bool GetServerName(std::string& out) const = 0;
};

/** Helper functions for obtaining TLS client certificates and key fingerprints
 * from StreamSockets
 */
class SSLClientCert
{
 public:
	/**
	 * Get the client certificate from a socket
	 * @param sock The socket to get the certificate from, the socket does not have to use TLS
	 * @return The TLS client certificate information, NULL if the peer is not using TLS
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
	 * socket does not have to use TLS
	 * @return The key fingerprint from the TLS certificate sent by the peer,
	 * empty if no cert was sent or the peer is not using TLS
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

	/** Get the TLS certificate of a user
	 * @param user The user whose certificate to get, user may be remote
	 * @return The TLS certificate of the user or NULL if the user is not using TLS
	 */
	virtual ssl_cert* GetCertificate(User* user) = 0;

	/** Set the TLS certificate of a user.
	 * @param user The user whose certificate to set.
	 * @param cert The TLS certificate to set for the user.
	 */
	virtual void SetCertificate(User* user, ssl_cert* cert) = 0;

	/** Get the key fingerprint from a user's certificate
	 * @param user The user whose key fingerprint to get, user may be remote
	 * @return The key fingerprint from the user's TLS certificate or an empty string
	 * if the user is not using TLS or did not provide a client certificate
	 */
	std::string GetFingerprint(User* user)
	{
		ssl_cert* cert = GetCertificate(user);
		if (cert)
			return cert->GetFingerprint();
		return "";
	}
};

/** API implemented by m_sslinfo that allows modules to retrieve the TLS certificate
 * information of local and remote users. It can also be used to find out whether a
 * user is using TLS or not.
 */
class UserCertificateAPI : public dynamic_reference<UserCertificateAPIBase>
{
 public:
	UserCertificateAPI(Module* parent)
		: dynamic_reference<UserCertificateAPIBase>(parent, "m_sslinfo_api")
	{
	}
};
