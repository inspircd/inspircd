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

#include "inspircd_test.h"

class stripcolor : public insptest::Test
{
 public:
	void SetUp() override
	{
		insptest::Test::SetUp();

		ASSERT_TRUE(inspircd->Modules->Load("stripcolor"));
	}
};

TEST_F(stripcolor, test1)
{
	Module *module = inspircd->Modules->Find("stripcolor");
	ASSERT_TRUE(module != nullptr);

	User *src = new insptest::User(),
	     *dst = new insptest::User();

	std::string text = "\00301,02hello\003";
	CUList exempt_list;
	module->OnUserPreMessage(src, dst, TYPE_USER, text, 0, exempt_list, MSG_PRIVMSG);

	ASSERT_EQ(text, "\00301,02hello\003");

	ModeHandler* mh = ServerInstance->Modes->FindMode('S', MODETYPE_USER);
	ASSERT_TRUE(mh != nullptr);

	dst->SetMode(mh, true);

	module->OnUserPreMessage(src, dst, TYPE_USER, text, 0, exempt_list, MSG_PRIVMSG);
	ASSERT_EQ(text, "hello");
}
