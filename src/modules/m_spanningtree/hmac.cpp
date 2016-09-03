/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
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

void TreeSocket::SetOurChallenge(const std::string &c)
{
	capab->ourchallenge = c;
}

const std::string& TreeSocket::GetTheirChallenge()
{
	return capab->theirchallenge;
}

void TreeSocket::SetTheirChallenge(const std::string &c)
{
	capab->theirchallenge = c;
}

std::string TreeSocket::MakePass(const std::string &password, const std::string &challenge)
{
	/* This is a simple (maybe a bit hacky?) HMAC algorithm, thanks to jilles for
	 * suggesting the use of HMAC to secure the password against various attacks.
	 *
	 * Note: If an sha256 provider is not available, we MUST fall back to plaintext with no
	 *       HMAC challenge/response.
	 */
	HashProvider* sha256 = ServerInstance->Modules->FindDataService<HashProvider>("hash/sha256");
	if (sha256 && !challenge.empty())
		return "AUTH:" + BinToBase64(sha256->hmac(password, challenge));

	if (!challenge.empty() && !sha256)
		ServerInstance->Logs->Log(MODNAME, LOG_DEFAULT, "Not authenticating to server using SHA256/HMAC because we don't have an SHA256 provider (e.g. m_sha256) loaded!");

	return password;
}

bool TreeSocket::ComparePass(const Link& link, const std::string &theirs)
{
	capab->auth_fingerprint = !link.Fingerprint.empty();
	capab->auth_challenge = !capab->ourchallenge.empty() && !capab->theirchallenge.empty();

	std::string fp = SSLClientCert::GetFingerprint(this);
	if (capab->auth_fingerprint)
	{
		/* Require fingerprint to exist and match */
		if (link.Fingerprint != fp)
		{
			ServerInstance->SNO->WriteToSnoMask('l',"Invalid SSL certificate fingerprint on link %s: need \"%s\" got \"%s\"",
				link.Name.c_str(), link.Fingerprint.c_str(), fp.c_str());
			SendError("Invalid SSL certificate fingerprint " + fp + " - expected " + link.Fingerprint);
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

	// Tell opers to set up fingerprint verification if it's not already set up and the SSL mod gave us a fingerprint
	// this time
	if ((!capab->auth_fingerprint) && (!fp.empty()))
	{
		ServerInstance->SNO->WriteToSnoMask('l', "SSL certificate fingerprint for link %s is \"%s\". "
			"You can improve security by specifying this in <link:fingerprint>.", link.Name.c_str(), fp.c_str());
	}

	return true;
}
