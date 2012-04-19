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
#include "socket.h"
#include "xline.h"
#include "../hash.h"
#include "../ssl.h"
#include "socketengine.h"

#include "main.h"
#include "utils.h"
#include "treeserver.h"
#include "link.h"
#include "treesocket.h"
#include "resolvers.h"

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
	 * Note: If m_sha256.so is not loaded, we MUST fall back to plaintext with no
	 *       HMAC challenge/response.
	 */
	HashProvider* sha256 = ServerInstance->Modules->FindDataService<HashProvider>("hash/sha256");
	if (Utils->ChallengeResponse && sha256 && !challenge.empty())
	{
		if (proto_version < 1202)
		{
			/* This is how HMAC is done in InspIRCd 1.2:
			 *
			 * sha256( (pass xor 0x5c) + sha256((pass xor 0x36) + m) )
			 *
			 * 5c and 36 were chosen as part of the HMAC standard, because they
			 * flip the bits in a way likely to strengthen the function.
			 */
			std::string hmac1, hmac2;

			for (size_t n = 0; n < password.length(); n++)
			{
				hmac1.push_back(static_cast<char>(password[n] ^ 0x5C));
				hmac2.push_back(static_cast<char>(password[n] ^ 0x36));
			}

			hmac2.append(challenge);
			hmac2 = sha256->hexsum(hmac2);
		
			std::string hmac = hmac1 + hmac2;
			hmac = sha256->hexsum(hmac);

			return "HMAC-SHA256:"+ hmac;
		}
		else
		{
			return "AUTH:" + BinToBase64(sha256->hmac(password, challenge));
		}
	}
	else if (!challenge.empty() && !sha256)
		ServerInstance->Logs->Log("m_spanningtree",DEFAULT,"Not authenticating to server using SHA256/HMAC because we don't have m_sha256 loaded!");

	return password;
}

bool TreeSocket::ComparePass(const Link& link, const std::string &theirs)
{
	capab->auth_fingerprint = !link.Fingerprint.empty();
	capab->auth_challenge = !capab->ourchallenge.empty() && !capab->theirchallenge.empty();

	std::string fp;
	if (GetIOHook())
	{
		SocketCertificateRequest req(this, Utils->Creator);
		if (req.cert)
		{
			fp = req.cert->GetFingerprint();
		}
	}

	if (capab->auth_challenge)
	{
		std::string our_hmac = MakePass(link.RecvPass, capab->ourchallenge);

		/* Straight string compare of hashes */
		if (our_hmac != theirs)
			return false;
	}
	else
	{
		/* Straight string compare of plaintext */
		if (link.RecvPass != theirs)
			return false;
	}

	if (capab->auth_fingerprint)
	{
		/* Require fingerprint to exist and match */
		if (link.Fingerprint != fp)
		{
			ServerInstance->SNO->WriteToSnoMask('l',"Invalid SSL fingerprint on link %s: need \"%s\" got \"%s\"",
				link.Name.c_str(), link.Fingerprint.c_str(), fp.c_str());
			SendError("Provided invalid SSL fingerprint " + fp + " - expected " + link.Fingerprint);
			return false;
		}
	}
	else if (!fp.empty())
	{
		ServerInstance->SNO->WriteToSnoMask('l', "SSL fingerprint for link %s is \"%s\". "
			"You can improve security by specifying this in <link:fingerprint>.", link.Name.c_str(), fp.c_str());
	}
	return true;
}
