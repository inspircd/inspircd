/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2009 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2008 Craig Edwards <craigedwards@brainbox.cc>
 *   Copyright (C) 2008 Dennis Friis <peavey@inspircd.org>
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

class wild : public insptest::Test
{
};

/* Test that x matches y with match() */
#define WCTEST_TRUE(x, y) ASSERT_TRUE(InspIRCd::Match(x, y, NULL))
/* Test that x does not match y with match() */
#define WCTEST_NOT(x, y) ASSERT_FALSE(InspIRCd::Match(x, y, NULL))

/* Test that x matches y with match() and cidr enabled */
#define CIDRTEST_TRUE(x, y) ASSERT_TRUE(InspIRCd::MatchCIDR(x, y, NULL))
/* Test that x does not match y with match() and cidr enabled */
#define CIDRTEST_NOT(x, y) ASSERT_FALSE(InspIRCd::MatchCIDR(x, y, NULL))

TEST_F(wild, test1)
{
	WCTEST_TRUE("foobar", "*");
	WCTEST_TRUE("foobar", "foo*");
	WCTEST_TRUE("foobar", "*bar");
	WCTEST_TRUE("foobar", "foo??r");
	WCTEST_TRUE("foobar.test", "fo?bar.*t");
	WCTEST_TRUE("foobar.test", "fo?bar.t*t");
	WCTEST_TRUE("foobar.tttt", "fo?bar.**t");
	WCTEST_TRUE("foobar", "foobar");
	WCTEST_TRUE("foobar", "foo***bar");
	WCTEST_TRUE("foobar", "*foo***bar");
	WCTEST_TRUE("foobar", "**foo***bar");
	WCTEST_TRUE("foobar", "**foobar*");
	WCTEST_TRUE("foobar", "**foobar**");
	WCTEST_TRUE("foobar", "**foobar");
	WCTEST_TRUE("foobar", "**f?*?ar");
	WCTEST_TRUE("foobar", "**f?*b?r");
	WCTEST_TRUE("foofar", "**f?*f*r");
	WCTEST_TRUE("foofar", "**f?*f*?");
	WCTEST_TRUE("r", "*");
	WCTEST_TRUE("", "");
	WCTEST_TRUE("test@foo.bar.test", "*@*.bar.test");
	WCTEST_TRUE("test@foo.bar.test", "*test*@*.bar.test");
	WCTEST_TRUE("test@foo.bar.test", "*@*test");

	WCTEST_TRUE("a", "*a");
	WCTEST_TRUE("aa", "*a");
	WCTEST_TRUE("aaa", "*a");
	WCTEST_TRUE("aaaa", "*a");
	WCTEST_TRUE("aaaaa", "*a");
	WCTEST_TRUE("aaaaaa", "*a");
	WCTEST_TRUE("aaaaaaa", "*a");
	WCTEST_TRUE("aaaaaaaa", "*a");
	WCTEST_TRUE("aaaaaaaaa", "*a");
	WCTEST_TRUE("aaaaaaaaaa", "*a");
	WCTEST_TRUE("aaaaaaaaaaa", "*a");

	WCTEST_NOT("foobar", "bazqux");
	WCTEST_NOT("foobar", "*qux");
	WCTEST_NOT("foobar", "foo*x");
	WCTEST_NOT("foobar", "baz*");
	WCTEST_NOT("foobar", "foo???r");
	WCTEST_NOT("foobar", "foobars");
	WCTEST_NOT("foobar", "**foobar**h");
	WCTEST_NOT("foobar", "**foobar**h*");
	WCTEST_NOT("foobar", "**f??*bar?");
	WCTEST_NOT("foobar", "");
	WCTEST_NOT("", "foobar");
	WCTEST_NOT("OperServ", "O");
	WCTEST_NOT("O", "OperServ");
	WCTEST_NOT("foobar.tst", "fo?bar.*g");
	WCTEST_NOT("foobar.test", "fo?bar.*tt");

	CIDRTEST_TRUE("brain@1.2.3.4", "*@1.2.0.0/16");
	CIDRTEST_TRUE("brain@1.2.3.4", "*@1.2.3.0/24");
	CIDRTEST_TRUE("192.168.3.97", "192.168.3.0/24");

	CIDRTEST_NOT("brain@1.2.3.4", "x*@1.2.0.0/16");
	CIDRTEST_NOT("brain@1.2.3.4", "*@1.3.4.0/24");
	CIDRTEST_NOT("1.2.3.4", "1.2.4.0/24");
	CIDRTEST_NOT("brain@1.2.3.4", "*@/24");
	CIDRTEST_NOT("brain@1.2.3.4", "@1.2.3.4/9");
	CIDRTEST_NOT("brain@1.2.3.4", "@");
	CIDRTEST_NOT("brain@1.2.3.4", "");
}

