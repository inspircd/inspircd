/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2020 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2019 B00mX0r <b00mx0r@aureus.pw>
 *   Copyright (C) 2018 Dylan Frank <b00mx0r@aureus.pw>
 *   Copyright (C) 2013, 2017-2024 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2013, 2015-2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
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

#include "iohook.h"

/** ssl_cert is a class which abstracts TLS certificate
 * and key information.
 *
 * Because gnutls and openssl represent key information in
 * wildly different ways, this class allows it to be accessed
 * in a unified manner. These classes are attached to ssl-
 * connected local users using SSLCertExt
 */
class ssl_cert final
	: public refcountbase
{
public:
	std::string dn;
	std::string issuer;
	std::string error;
	std::vector<std::string> fingerprints;
	bool trusted = false;
	bool invalid = true;
	bool unknownsigner = true;
	bool revoked = false;
	time_t activation = 0;
	time_t expiration = 0;


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

	/** Get primary fingerprint.
	 * @return The primary fingerprint as a hex string.
	 */
	std::string GetFingerprint() const
	{
		return fingerprints.empty() ? "" : fingerprints.front();
	}

	/** Get all fingerprints.
	 * @return All fingerprints as a hex string.
	 */
	const std::vector<std::string>& GetFingerprints() const
	{
		return fingerprints;
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

	/** Retrieves the client certificate activation time.
	 * @return The time the client certificate was activated or 0 on error.
	 */
	time_t GetActivationTime() const
	{
		return activation;
	}

	/** Retrieves the client certificate expiration time.
	 * @return The time the client certificate will expire or 0 on error.
	 */
	time_t GetExpirationTime() const
	{
		return expiration;
	}
};

/** I/O hook provider for TLS modules. */
class SSLIOHookProvider
	: public IOHookProvider
{
public:
	SSLIOHookProvider(Module* mod, const std::string& Name)
		: IOHookProvider(mod, "ssl/" + Name, IOH_SSL)
	{
	}
};

class SSLIOHook
	: public IOHook
{
protected:
	/** An enumeration of possible TLS socket states. */
	enum Status
	{
		/** The TLS socket has just been opened or has been closed. */
		STATUS_NONE,

		/** The TLS socket is currently handshaking. */
		STATUS_HANDSHAKING,

		/** The TLS handshake has completed and data can be sent. */
		STATUS_OPEN
	};

	/** Peer TLS certificate, set by the TLS module
	 */
	reference<ssl_cert> certificate;

	/** The status of the TLS connection. */
	Status status = STATUS_NONE;

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

		return nullptr;
	}

	SSLIOHook(const std::shared_ptr<IOHookProvider>& hookprov)
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

	/** @copydoc IOHook::IsHookReady */
	bool IsHookReady() const override { return status == STATUS_OPEN; }
};

class UserCertificateAPIBase
	: public DataProvider
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

	/** Determines whether the specified user is connected securely.
	 * @return True if the user is connected securely; otherwise, false.
	 */
	virtual bool IsSecure(User* user) = 0;

	/** Set the TLS certificate of a user.
	 * @param user The user whose certificate to set.
	 * @param cert The TLS certificate to set for the user.
	 */
	virtual void SetCertificate(User* user, ssl_cert* cert) = 0;

	/** Get the primary fingerprint from a user's certificate
	 * @param user The user whose primary fingerprint to get.
	 * @return The primary fingerprint from the user's TLS certificate or an empty string
	 * if the user is not using TLS or did not provide a client certificate
	 */
	std::string GetFingerprint(User* user)
	{
		ssl_cert* cert = GetCertificate(user);
		if (cert && cert->IsUsable())
			return cert->GetFingerprint();
		return "";
	}

	/** Get all fingerprint from a user's certificate
	 * @param user The user whose fingerprint to get.
	 * @return The fingerprint from the user's TLS certificate or an empty string
	 * if the user is not using TLS or did not provide a client certificate
	 */
	std::vector<std::string> GetFingerprints(User* user)
	{
		ssl_cert* cert = GetCertificate(user);
		if (cert && cert->IsUsable())
			return cert->GetFingerprints();
		return {};
	}
};

/** API implemented by m_sslinfo that allows modules to retrieve the TLS certificate
 * information of local and remote users. It can also be used to find out whether a
 * user is using TLS or not.
 */
class UserCertificateAPI final
	: public dynamic_reference<UserCertificateAPIBase>
{
public:
	UserCertificateAPI(Module* parent)
		: dynamic_reference<UserCertificateAPIBase>(parent, "m_sslinfo_api")
	{
	}
};
