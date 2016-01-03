/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2016 Adam <Adam@anope.org>
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

#include "inspircd.h"
#include <gtest/gtest.h>

namespace insptest
{
	class Test : public testing::Test
	{
	 protected:
		InspIRCd *inspircd;

		void SetUp() override
		{
			const char *argv[] = { "inspircd", "--nofork", "--quiet", NULL };
			inspircd = new InspIRCd(3, (char**) argv);
		}

		void TearDown() override
		{
			inspircd->Cleanup();
			delete inspircd;
		}
	};

	class User : public LocalUser
	{
		static irc::sockets::sockaddrs zero;

	 public:
		User() : LocalUser(-1, &zero, &zero) { }
	};
}

