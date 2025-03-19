/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2020 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2018-2022 Sadie Powell <sadie@witchery.services>
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

namespace
{
	sig_atomic_t lastsignum = 0;

	void SignalHandler(int signum)
	{
		lastsignum = signum;
	}
}

class ModuleRehashSignal final
	: public Module
{
private:
	using SignalMap = std::multimap<int, std::string>;
	SignalMap signals;

	static int NameToSignal(std::string signame)
	{
		if (signame.find_first_not_of("0123456789") == std::string::npos)
			return ConvToNum(signame, -1);

		std::transform(signame.begin(), signame.end(), signame.begin(), ::toupper);
		if (signame == "SIGUSR1" || signame == "USR1")
			return SIGUSR1;
		if (signame == "SIGUSR2" || signame == "USR2")
			return SIGUSR2;

#if defined(SIGRTMIN) && defined(SIGRTMAX)
		if (signame == "SIGRTMIN" || signame == "RTMIN")
			return SIGRTMIN;
		if (signame == "SIGRTMAX" || signame == "RTMAX")
			return SIGRTMAX;

		int rtnum = -1;
		if (signame.compare(0, 9, "SIGRTMIN+") == 0)
			rtnum = SIGRTMIN + ConvToNum<int>(signame.substr(9), -1);
		else if (signame.compare(0, 6, "RTMIN+") == 0)
			rtnum = SIGRTMIN + ConvToNum<int>(signame.substr(6), -1);
		else if (signame.compare(0, 9, "SIGRTMAX-") == 0)
			rtnum = SIGRTMAX - ConvToNum<int>(signame.substr(9), -1);
		else if (signame.compare(0, 6, "RTMAX-") == 0)
			rtnum = SIGRTMAX - ConvToNum<int>(signame.substr(6), -1);

		if (rtnum >= SIGRTMIN && rtnum <= SIGRTMAX)
			return rtnum;
#endif

		return -1; // No such signal.
	}

	static std::string SignalToName(int signum)
	{
#ifdef HAS_SIGABBREV_NP
		const auto *signame = sigabbrev_np(signum);
		if (signame)
			return FMT::format("SIG{}", signame);
#endif
		return FMT::format("signal {}", signum);
	}

public:
	 ModuleRehashSignal()
		: Module(VF_VENDOR, "Allows signals to be sent to the server to reload module data files.")
	{
	}

	~ModuleRehashSignal() override
	{
		for (const auto &[signum, _] : signals)
			signal(signum, SIG_IGN);
	}

	void ReadConfig(ConfigStatus& status) override
	{
		SignalMap newsignals;
		for (const auto& [_, tag] : ServerInstance->Config->ConfTags("rehashsignal"))
		{
			const auto signame = tag->getString("signal");
			if (signame.empty())
				throw ModuleException(this, "<rehashsignal:signal> must not be empty, at " + tag->source.str());

			const auto signum = NameToSignal(signame);
			if (signum < 0)
				throw ModuleException(this, "<rehashsignal:signal> must a realtime or user signal, at " + tag->source.str());

			const auto rehash = tag->getString("rehash");
			if (rehash.empty())
				throw ModuleException(this, "<rehashsignal:rehash> must not be empty, at " + tag->source.str());

			ServerInstance->Logs.Debug(MODNAME, "Signal rehash: {} => {}", SignalToName(signum), rehash);
			newsignals.emplace(signum, rehash);
		}

		if (newsignals.empty())
		{
			ServerInstance->Logs.Debug(MODNAME, "No signals configured, defaulting to SIGUSR1 => tls");
			newsignals.emplace(SIGUSR1, "tls");
		}

		// Remove the old signal handlers and apply the new ones.
		for (const auto &[signum, _] : signals)
			signal(signum, SIG_IGN);
		std::swap(signals, newsignals);
		for (const auto &[signum, _] : signals)
			signal(signum, SignalHandler);
	}

	void OnBackgroundTimer(time_t) override
	{
		if (!lastsignum)
			return;

		for (const auto& [signum, rehash] : insp::equal_range(signals, lastsignum))
		{
			const auto feedbackmsg = FMT::format("Received {}, performing a module rehash: {}",
				SignalToName(signum), rehash);

			ServerInstance->SNO.WriteGlobalSno('r', feedbackmsg);
			ServerInstance->Logs.Normal(MODNAME, feedbackmsg);

			FOREACH_MOD(OnModuleRehash, (nullptr, rehash));
		}
		lastsignum = 0;
	}
};

MODULE_INIT(ModuleRehashSignal)
