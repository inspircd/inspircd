/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2026 Sadie Powell <sadie@witchery.services>
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

namespace TLS
{
	class API;
	class APIBase;
	class Certificate;
	class IOHook;

	using CertificatePtr = std::shared_ptr<Certificate>;
	using FingerprintList = std::vector<std::string>;

	inline IOHook* GetHook(StreamSocket* sock);
}

class TLS::Certificate
	: public std::enable_shared_from_this<TLS::Certificate>
{
protected:
	Certificate() = default;

	/** A list of zero or more TLS certificate fingerprints. */
	TLS::FingerprintList fingerprints;

	/** The Distinguished Name of the TLS certificate. */
	std::string dn;

	/** If non-empty then the error which occurred whilst creating this \ref TLS::Certificate. */
	std::string error;

	/** The Distinguished Name of the TLS certificate's issuer. */
	std::string issuer_dn;

	/** The time at which the TLS certificate was activated. */
	time_t activation = 0;

	/** The time at which the TLS certificate will expire. */
	time_t expiration = 0;

	/** Whether the signer of the TLS certificate is known. */
	bool known_signer:1 = false;

	/* Whether the TLS certificate has been revoked. */
	bool revoked:1 = false;

	/** Whether the TLS certificate is trusted by the system certificate store. */
	bool trusted:1 = false;

	/** Whether the TLS certificate is valid. */
	bool valid:1 = false;

public:
	/** Retrieves the time at which the TLS certificate was activated, or 0 if the peer did not
	 * provide a TLS certificate.
	 */
	inline auto GetActivationTime() const { return this->activation; }

	/** Retrieves the Distinguished Name of the TLS certificate. */
	inline const auto& GetDN() const { return this->dn; }

	/** If an error occurred whilst creating this \ref TLS::Certificate then the error; otherwise,
	 * an empty string. Call \ref HasError instead if you just want to check if an error occurred.
	 */
	inline const auto& GetError() const { return this->error; }

	/** Retrieves the time at which the TLS certificate will expire, or 0 if the peer did not
	 * provide a TLS certificate.
	 */
	inline auto GetExpirationTime() const { return this->expiration; }

	/** Retrieves all available fingerprints. */
	inline const auto& GetFingerprints() const { return this->fingerprints; }

	/** Retrieves the primary fingerprint. */
	inline auto GetFingerprint() const
	{
		return GetFingerprints().empty() ? "" : GetFingerprints().front();
	}

	/** Retrieves the Distinguished Name of the TLS certificate's issuer. */
	inline const auto& GetIssuerDN() const { return this->issuer_dn; }

	/** Whether an error occurred whilst creating this \ref TLS::Certificate. */
	inline auto HasError() const { return !this->error.empty(); }

	/** Determines whether the signer of the TLS certificate is known. */
	inline auto IsKnownSigner() const { return this->known_signer; }

	/* Determines whether the TLS certificate has been revoked. */
	inline auto IsRevoked() const { return this->revoked; }

	/** Determines whether the TLS certificate is trusted by the system certificate store. */
	inline auto IsTrusted() const { return this->trusted; }

	/** Determines whether the TLS certificate is valid. */
	inline auto IsValid() const { return this->valid; }

	/** Determines whether this TLS certificate is usable.
	 * @param strict Whether to consider self-signed TLS certificates as invalid.
	 */
	inline auto IsUsable(bool strict = false) const
	{
		if (strict && (!IsTrusted() || !IsKnownSigner()))
			return false;

		return !HasError() && !IsRevoked() && IsValid();
	}
};

class TLS::APIBase
	: public Service::SimpleProvider
{
public:
	APIBase(const WeakModulePtr& mod)
		: Service::SimpleProvider(mod, "tlsapi")
	{
	}

	/** Retrieves the TLS certificate of a user.
	 * @param user The user to retrieve the TLS certificate of.
	 * @return The TLS certificate of the user or a null pointer if they are not using TLS.
	 */
	virtual const Certificate* GetCertificate(User* user) = 0;

	/** Retrieves the primary TLS fingerprint of a user.
	 * @param user The user to retrieve the primary TLS fingerprint of.
	 * @param strict Whether to consider self-signed TLS certificates as invalid.
	 * @return The primary TLS fingerprint of the user or an empty string if \p user did not provide
	 *         a TLS certificate.
	 */
	inline auto GetFingerprint(User* user, bool strict = false)
	{
		const auto* cert = GetCertificate(user);
		if (cert && cert->IsUsable(strict)) [[likely]]
			return cert->GetFingerprint();
		return std::string();
	}

	/** Retrieves all available TLS fingerprint of a user.
	 * @param user The user to retrieve all available TLS fingerprints of.
	 * @param strict Whether to consider self-signed TLS certificates as invalid.
	 * @return A list of TLS fingerprints or an empty list if \p user did not provide a TLS
	 *         certificate.
	 */
	inline auto GetFingerprints(User* user, bool strict = false)
	{
		const auto* cert = GetCertificate(user);
		if (cert && cert->IsUsable(strict)) [[likely]]
			return cert->GetFingerprints();
		return TLS::FingerprintList();
	}

	/** Determines whether the specified user is connected securely. A secure connection is defined
	 * as using TLS or connecting locally to the server.
	 * @return True if the user is connected securely; otherwise, false.
	 */
	virtual bool IsSecure(User* user) = 0;

	/** Sets the TLS certificate of a user.
	 * @param user The user whose TLS certificate to set.
	 * @param cert The TLS certificate to set for the user.
	 */
	virtual void SetCertificate(User* user, const TLS::CertificatePtr& cert) = 0;
};

class TLS::API final
	: public dynamic_reference<TLS::APIBase>
{
public:
	API(const WeakModulePtr& mod)
		: dynamic_reference<TLS::APIBase>(mod, "tlsapi")
	{
	}
};

class TLS::IOHook
	: public ::IOHook
{
protected:
	/** An enumeration of possible TLS socket states. */
	enum Status
		: uint8_t
	{
		/** The TLS socket has just been opened or has been closed. */
		STATUS_NONE,

		/** The TLS socket is currently handshaking. */
		STATUS_HANDSHAKING,

		/** The TLS handshake has completed and data can be sent. */
		STATUS_OPEN
	};

	/** The TLS certificate of the peer. */
	TLS::CertificatePtr certificate;

	/** The status of the TLS connection. */
	Status status = STATUS_NONE;

public:
	IOHook(const std::shared_ptr<IOHookProvider>& hookprov)
		: ::IOHook(hookprov)
	{
	}

	/** Retrieves the TLS certificate of the peer. */
	const auto& GetCertificate() const
	{
		return this->certificate;
	}

	/** Retrieves the ciphersuite that was negotiated with the peer.
	 * @param out The location that the ciphersuite will be stored.
	 * @return True if the ciphersuite was retrieved; otherwise, false.
	 */
	virtual bool GetCiphersuite(std::string& out) const = 0;

	/** Retrieves the hostname that was sent by the peer using SNI.
	 * @param out The location that the server name will be stored.
	 * @return True if the server name was retrieved; otherwise, false.
	 */
	virtual bool GetServerName(std::string& out) const = 0;

	/** @copydoc IOHook::IsHookReady */
	bool IsHookReady() const override { return this->status == STATUS_OPEN; }
};

TLS::IOHook* TLS::GetHook(StreamSocket* sock)
{
	if (!sock)
		return nullptr; // No socket.

	auto* const lasthook = sock->GetLastHook();
	if (!lasthook || !lasthook->prov->service_name.starts_with("ssl/"))
		return nullptr; // Not our hook.

	return static_cast<TLS::IOHook*>(lasthook);
}
