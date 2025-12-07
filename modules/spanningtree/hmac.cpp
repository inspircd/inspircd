/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2014 Matthew Martin <phy1729@gmail.com>
 *   Copyright (C) 2013-2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2013, 2019, 2021-2023, 2025 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
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


#include "inspircd.h"
#include "modules/hash.h"
#include "modules/ssl.h"

#include "main.h"
#include "link.h"
#include "treesocket.h"

const std::string& TreeSocket::GetOurChallenge()
{
	return capab->ourchallenge;
}

void TreeSocket::SetOurChallenge(const std::string& c)
{
	capab->ourchallenge = c;
}

const std::string& TreeSocket::GetTheirChallenge()
{
	return capab->theirchallenge;
}

void TreeSocket::SetTheirChallenge(const std::string& c)
{
	capab->theirchallenge = c;
}

std::string TreeSocket::MakePass(const std::string& password, const std::string& challenge)
{
	/* This is a simple (maybe a bit hacky?) HMAC algorithm, thanks to jilles for
	 * suggesting the use of HMAC to secure the password against various attacks.
	 *
	 * Note: If an sha256 provider is not available, we MUST fall back to plaintext with no
	 *       HMAC challenge/response.
	 */
	auto* sha256 = ServerInstance->Modules.FindDataService<Hash::Provider>("hash/sha256");
	if (sha256 && !challenge.empty())
		return "AUTH:" + Base64::Encode(Hash::HMAC(sha256, password, challenge));

	if (!challenge.empty() && !sha256)
		ServerInstance->Logs.Warning(MODNAME, "Not authenticating to server using HMAC-SHA256 because we don't have an SHA256 provider (e.g. the sha2 module) loaded!");

	return password;
}

bool TreeSocket::ComparePass(const Link& link, const std::string& theirs)
{
	capab->auth_fingerprint = !link.Fingerprint.empty();
	capab->auth_challenge = !capab->ourchallenge.empty() && !capab->theirchallenge.empty();

	const auto* sslhook = SSLIOHook::IsSSL(this);
	const auto* sslcert = sslhook ? sslhook->GetCertificate() : nullptr;
	const auto sslcert_usable = sslcert && sslcert->IsUsable();
	const auto fp = sslcert_usable ? sslcert->GetFingerprint() : "";
	if (capab->auth_fingerprint)
	{
		std::string sslerror;
		if (!sslhook)
			sslerror = "not using TLS";
		if (!sslcert)
			sslerror = "not using a TLS client certificate";
		else if (!sslcert_usable)
			sslerror = "using an invalid (probably expired) TLS client certificate";
		else
		{
			std::string badfps;
			auto foundfp = false;
			for (const auto& fingerprint : sslcert->GetFingerprints())
			{
				if (InspIRCd::TimingSafeCompare(link.Fingerprint, fingerprint))
				{
					foundfp = true;
					break;
				}
				badfps.append(badfps.empty() ? "" : ", ").append(fingerprint);
			}
			if (!foundfp)
			{
				sslerror = INSP_FORMAT("not using the correct TLS client certificate (need \"{}\" got \"{}\")",
					link.Fingerprint, badfps.empty() ? "(none)" : badfps);
			}
		}

		/* Require fingerprint to exist and match */
		if (!sslerror.empty())
		{
			ServerInstance->SNO.WriteToSnoMask('l', "Incorrect TLS client certificate on link {}: {}",
				link.Name, sslerror);
			SendError("Incorrect TLS client certificate: " + sslerror);
			return false;
		}
	}

	if (capab->auth_challenge)
	{
		std::string our_hmac = MakePass(link.RecvPass, capab->ourchallenge);

		// Use the timing-safe compare function to compare the hashes
		if (!InspIRCd::TimingSafeCompare(our_hmac, theirs))
			return false;
	}
	else
	{
		// Use the timing-safe compare function to compare the passwords
		if (!InspIRCd::TimingSafeCompare(link.RecvPass, theirs))
			return false;
	}

	// Tell opers to set up fingerprint verification if it's not already set up and the TLS mod gave us a fingerprint
	// this time
	if ((!capab->auth_fingerprint) && (!fp.empty()))
	{
		ServerInstance->SNO.WriteToSnoMask('l', "TLS client certificate fingerprint for link {} is \"{}\". "
			"You can improve security by specifying this in <link:fingerprint>.", link.Name, fp);
	}

	return true;
}
