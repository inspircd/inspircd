/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2020 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2017-2021 Sadie Powell <sadie@witchery.services>
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
#include "modules/cap.h"
#include "modules/ssl.h"

class STSCap : public Cap::Capability
{
 private:
	std::string host;
	std::string plaintextpolicy;
	std::string securepolicy;

	bool OnList(LocalUser* user) CXX11_OVERRIDE
	{
		// Don't send the cap to clients that only support cap-3.1.
		if (GetProtocol(user) == Cap::CAP_LEGACY)
			return false;

		// Don't send the cap to clients in a class which has STS disabled.
		if (!user->GetClass()->config->getBool("usests", true))
			return false;

		// Plaintext listeners have their own policy.
		SSLIOHook* sslhook = SSLIOHook::IsSSL(&user->eh);
		if (!sslhook)
			return true;

		// If no hostname has been provided for the connection, an STS persistence policy SHOULD NOT be advertised.
		std::string snihost;
		if (!sslhook->GetServerName(snihost))
			return false;

		// Before advertising an STS persistence policy over a secure connection, servers SHOULD verify whether the
		// hostname provided by clients, for example, via TLS Server Name Indication (SNI), has been whitelisted by
		// administrators in the server configuration.
		return InspIRCd::Match(snihost, host, ascii_case_insensitive_map);
	}

	bool OnRequest(LocalUser* user, bool adding) CXX11_OVERRIDE
	{
		// Clients MUST NOT request this capability with CAP REQ. Servers MAY reply with a CAP NAK message if a
		// client requests this capability.
		return false;
	}

	const std::string* GetValue(LocalUser* user) const CXX11_OVERRIDE
	{
		return SSLIOHook::IsSSL(&user->eh) ? &securepolicy : &plaintextpolicy;
	}

 public:
	STSCap(Module* mod)
		: Cap::Capability(mod, "sts")
	{
		DisableAutoRegister();
	}

	~STSCap()
	{
		// TODO: Send duration=0 when STS vanishes.
	}

	void SetPolicy(const std::string& newhost, unsigned long duration, unsigned int port, bool preload)
	{
		// To enforce an STS upgrade policy, servers MUST send this key to insecurely connected clients. Servers
		// MAY send this key to securely connected clients, but it will be ignored.
		std::string newplaintextpolicy("port=");
		newplaintextpolicy.append(ConvToStr(port));

		// To enforce an STS persistence policy, servers MUST send this key to securely connected clients. Servers
		// MAY send this key to all clients, but insecurely connected clients MUST ignore it.
		std::string newsecurepolicy("duration=");
		newsecurepolicy.append(ConvToStr(duration));

		// Servers MAY send this key to all clients, but insecurely connected clients MUST ignore it.
		if (preload)
			newsecurepolicy.append(",preload");

		// Apply the new policy.
		bool changed = false;
		if (!irc::equals(host, newhost))
		{
			ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Changing STS SNI hostname from \"%s\" to \"%s\"", host.c_str(), newhost.c_str());
			host = newhost;
			changed = true;
		}

		if (plaintextpolicy != newplaintextpolicy)
		{
			ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Changing plaintext STS policy from \"%s\" to \"%s\"", plaintextpolicy.c_str(), newplaintextpolicy.c_str());
			plaintextpolicy.swap(newplaintextpolicy);
			changed = true;
		}

		if (securepolicy != newsecurepolicy)
		{
			ServerInstance->Logs->Log(MODNAME, LOG_DEBUG, "Changing secure STS policy from \"%s\" to \"%s\"", securepolicy.c_str(), newsecurepolicy.c_str());
			securepolicy.swap(newsecurepolicy);
			changed = true;
		}

		// If the policy has changed then notify all clients via cap-notify.
		if (changed)
			NotifyValueChange();
	}
};

class ModuleIRCv3STS : public Module
{
 private:
	STSCap cap;

	// The IRCv3 STS specification requires that the server is listening using TLS (SSL) using a valid certificate.
	bool HasValidSSLPort(unsigned int port)
	{
		for (std::vector<ListenSocket*>::const_iterator iter = ServerInstance->ports.begin(); iter != ServerInstance->ports.end(); ++iter)
		{
			ListenSocket* ls = *iter;

			// Is this listener on the right port?
			unsigned int saport = ls->bind_sa.port();
			if (saport != port)
				continue;

			// Is this listener using TLS (SSL)?
			if (ls->bind_tag->getString("sslprofile", ls->bind_tag->getString("ssl")).empty())
				continue;

			// TODO: Add a way to check if a listener's TLS cert is CA-verified.
			return true;
		}
		return false;
	}

 public:
	ModuleIRCv3STS()
		: cap(this)
	{
	}

	void ReadConfig(ConfigStatus& status) CXX11_OVERRIDE
	{
		// TODO: Multiple SNI profiles
		ConfigTag* tag = ServerInstance->Config->ConfValue("sts");
		if (tag == ServerInstance->Config->EmptyTag)
			throw ModuleException("You must define a STS policy!");

		const std::string host = tag->getString("host");
		if (host.empty())
			throw ModuleException("<sts:host> must contain a hostname, at " + tag->getTagLocation());

		unsigned int port = tag->getUInt("port", 0, 0, UINT16_MAX);
		if (!HasValidSSLPort(port))
			throw ModuleException("<sts:port> must be a TLS port, at " + tag->getTagLocation());

		unsigned long duration = tag->getDuration("duration", 5*60, 60);
		bool preload = tag->getBool("preload");
		cap.SetPolicy(host, duration, port, preload);

		if (!cap.IsRegistered())
			ServerInstance->Modules->AddService(cap);
	}

	Version GetVersion() CXX11_OVERRIDE
	{
		return Version("Adds support for the IRCv3 Strict Transport Security specification.", VF_OPTCOMMON|VF_VENDOR);
	}
};

MODULE_INIT(ModuleIRCv3STS)
