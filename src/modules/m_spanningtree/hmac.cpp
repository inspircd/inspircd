/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "socket.h"
#include "xline.h"
#include "../transport.h"
#include "../m_hash.h"
#include "socketengine.h"

#include "main.h"
#include "utils.h"
#include "treeserver.h"
#include "link.h"
#include "treesocket.h"
#include "resolvers.h"
#include "handshaketimer.h"

/* $ModDep: m_spanningtree/resolvers.h m_spanningtree/main.h m_spanningtree/utils.h m_spanningtree/treeserver.h m_spanningtree/link.h m_spanningtree/treesocket.h m_hash.h */

const std::string& TreeSocket::GetOurChallenge()
{
	return this->ourchallenge;
}

void TreeSocket::SetOurChallenge(const std::string &c)
{
	this->ourchallenge = c;
}

const std::string& TreeSocket::GetTheirChallenge()
{
	return this->theirchallenge;
}

void TreeSocket::SetTheirChallenge(const std::string &c)
{
	this->theirchallenge = c;
}

std::string TreeSocket::MakePass(const std::string &password, const std::string &challenge)
{
	/* This is a simple (maybe a bit hacky?) HMAC algorithm, thanks to jilles for
	 * suggesting the use of HMAC to secure the password against various attacks.
	 *
	 * Note: If m_sha256.so is not loaded, we MUST fall back to plaintext with no
	 *       HMAC challenge/response.
	 */
	Module* sha256 = ServerInstance->Modules->Find("m_sha256.so");
	if (Utils->ChallengeResponse && sha256 && !challenge.empty())
	{
		/* XXX: This is how HMAC is supposed to be done:
		 *
		 * sha256( (pass xor 0x5c) + sha256((pass xor 0x36) + m) )
		 *
		 * Note that we are encoding the hex hash, not the binary
		 * output of the hash which is slightly different to standard.
		 *
		 * 5c and 36 were chosen as part of the HMAC standard, because they
		 * flip the bits in a way likely to strengthen the function.
		 */
		std::string hmac1, hmac2;

		for (size_t n = 0; n < password.length(); n++)
		{
			hmac1 += static_cast<char>(password[n] ^ 0x5C);
			hmac2 += static_cast<char>(password[n] ^ 0x36);
		}

		hmac2 += challenge;
		HashResetRequest(Utils->Creator, sha256).Send();
		hmac2 = HashSumRequest(Utils->Creator, sha256, hmac2).Send();

		HashResetRequest(Utils->Creator, sha256).Send();
		std::string hmac = hmac1 + hmac2;
		hmac = HashSumRequest(Utils->Creator, sha256, hmac).Send();

		return "HMAC-SHA256:"+ hmac;
	}
	else if (!challenge.empty() && !sha256)
		ServerInstance->Logs->Log("m_spanningtree",DEFAULT,"Not authenticating to server using SHA256/HMAC because we don't have m_sha256 loaded!");

	return password;
}

std::string TreeSocket::RandString(unsigned int ilength)
{
	char* randombuf = new char[ilength+1];
	std::string out;
#ifndef WINDOWS
	int f = open("/dev/urandom", O_RDONLY, 0);

	if (f >= 0)
	{
		if (read(f, randombuf, ilength) < ilength)
			ServerInstance->Logs->Log("m_spanningtree", DEFAULT, "Entropy source has gone predictable (did not return enough data)");
		close(f);
	}
	else
#endif
	{
		for (unsigned int i = 0; i < ilength; i++)
			randombuf[i] = rand();
	}

	for (unsigned int i = 0; i < ilength; i++)
	{
		char randchar = static_cast<char>(0x3F + (randombuf[i] & 0x3F));
		out += randchar;
	}

	delete[] randombuf;
	return out;
}

bool TreeSocket::ComparePass(const Link& link, const std::string &theirs)
{
	this->auth_fingerprint = !link.Fingerprint.empty();
	this->auth_challenge = !ourchallenge.empty() && !theirchallenge.empty();

	std::string fp;
	if (GetHook())
	{
		BufferedSocketCertificateRequest req(this, Utils->Creator, GetHook());
		req.Send();
		if (req.cert)
		{
			fp = req.cert->GetFingerprint();
			ServerInstance->Logs->Log("m_spanningtree", DEFAULT, std::string("Server SSL fingerprint ") + fp);
		}
	}

	if (auth_fingerprint)
	{
		/* Require fingerprint to exist and match */
		if (link.Fingerprint != fp)
			return false;
	}

	if (auth_challenge)
	{
		std::string our_hmac = MakePass(link.RecvPass, ourchallenge);

		/* Straight string compare of hashes */
		return our_hmac == theirs;
	}

	/* Straight string compare of plaintext */
	return link.RecvPass == theirs;
}
