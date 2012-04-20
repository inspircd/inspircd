/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2011 Jackmcbarn <jackmcbarn@jackmcbarn.no-ip.org>
 *   Copyright (C) 2010 Daniel De Graaf <danieldg@inspircd.org>
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


/* $Core */

#include "inspircd.h"
#include "cull_list.h"
#include "testsuite.h"
#include <iostream>

#define COUTFAILED() std::cout << std::endl << (failed ? "FAILURE" : "SUCCESS") << std::endl << std::endl

/* Test that x matches y with match() */
#define WCTEST(x, y) do { std::cout << "match(\"" << x << "\",\"" << y "\") " << ((testpassed = (InspIRCd::Match(x, y, NULL))) ? "SUCCESS" : "FAILURE") << std::endl; failed = !testpassed || failed; } while(0)
/* Test that x does not match y with match() */
#define WCTESTNOT(x, y) do { std::cout << "!match(\"" << x << "\",\"" << y "\") " << ((testpassed = ((!InspIRCd::Match(x, y, NULL)))) ? "SUCCESS" : "FAILURE") << std::endl; failed = !testpassed || failed; } while(0)

/* Test that x matches y with match() and cidr enabled */
#define CIDRTEST(x, y) do { std::cout << "match(\"" << x << "\",\"" << y "\", true) " << ((testpassed = (InspIRCd::MatchCIDR(x, y, NULL))) ? "SUCCESS" : "FAILURE") << std::endl; failed = !testpassed || failed; } while(0)
/* Test that x does not match y with match() and cidr enabled */
#define CIDRTESTNOT(x, y) do { std::cout << "!match(\"" << x << "\",\"" << y "\", true) " << ((testpassed = ((!InspIRCd::MatchCIDR(x, y, NULL)))) ? "SUCCESS" : "FAILURE") << std::endl; failed = !testpassed || failed; } while(0)

static bool DoWildTests()
{
	std::cout << "Wildcard and CIDR tests" << std::endl << std::endl;
	bool testpassed = false, failed = false;

	WCTEST("foobar", "*");
	WCTEST("foobar", "foo*");
	WCTEST("foobar", "*bar");
	WCTEST("foobar", "foo??r");
	WCTEST("foobar.test", "fo?bar.*t");
	WCTEST("foobar.test", "fo?bar.t*t");
	WCTEST("foobar.tttt", "fo?bar.**t");
	WCTEST("foobar", "foobar");
	WCTEST("foobar", "foo***bar");
	WCTEST("foobar", "*foo***bar");
	WCTEST("foobar", "**foo***bar");
	WCTEST("foobar", "**foobar*");
	WCTEST("foobar", "**foobar**");
	WCTEST("foobar", "**foobar");
	WCTEST("foobar", "**f?*?ar");
	WCTEST("foobar", "**f?*b?r");
	WCTEST("foofar", "**f?*f*r");
	WCTEST("foofar", "**f?*f*?");
	WCTEST("r", "*");
	WCTEST("", "");
	WCTEST("test@foo.bar.test", "*@*.bar.test");
	WCTEST("test@foo.bar.test", "*test*@*.bar.test");
	WCTEST("test@foo.bar.test", "*@*test");

	WCTEST("a", "*a");
	WCTEST("aa", "*a");
	WCTEST("aaa", "*a");
	WCTEST("aaaa", "*a");
	WCTEST("aaaaa", "*a");
	WCTEST("aaaaaa", "*a");
	WCTEST("aaaaaaa", "*a");
	WCTEST("aaaaaaaa", "*a");
	WCTEST("aaaaaaaaa", "*a");
	WCTEST("aaaaaaaaaa", "*a");
	WCTEST("aaaaaaaaaaa", "*a");

	WCTESTNOT("foobar", "bazqux");
	WCTESTNOT("foobar", "*qux");
	WCTESTNOT("foobar", "foo*x");
	WCTESTNOT("foobar", "baz*");
	WCTESTNOT("foobar", "foo???r");
	WCTESTNOT("foobar", "foobars");
	WCTESTNOT("foobar", "**foobar**h");
	WCTESTNOT("foobar", "**foobar**h*");
	WCTESTNOT("foobar", "**f??*bar?");
	WCTESTNOT("foobar", "");
	WCTESTNOT("", "foobar");
	WCTESTNOT("OperServ", "O");
	WCTESTNOT("O", "OperServ");
	WCTESTNOT("foobar.tst", "fo?bar.*g");
	WCTESTNOT("foobar.test", "fo?bar.*tt");

	CIDRTEST("brain@1.2.3.4", "*@1.2.0.0/16");
	CIDRTEST("brain@1.2.3.4", "*@1.2.3.0/24");
	CIDRTEST("192.168.3.97", "192.168.3.0/24");
	CIDRTEST("2001:db8::1", "2001:db8::/32");

	CIDRTESTNOT("brain@1.2.3.4", "x*@1.2.0.0/16");
	CIDRTESTNOT("brain@1.2.3.4", "*@1.3.4.0/24");
	CIDRTESTNOT("1.2.3.4", "1.2.4.0/24");
	CIDRTESTNOT("brain@1.2.3.4", "*@/24");
	CIDRTESTNOT("brain@1.2.3.4", "@1.2.3.4/9");
	CIDRTESTNOT("brain@1.2.3.4", "@");
	CIDRTESTNOT("brain@1.2.3.4", "");
	CIDRTESTNOT("2001:db8:aaaa::1", "2001:db8:bbbb::/48");

	std::cout << std::endl << "Result of wildcard and CIDR tests:";
	COUTFAILED();
	return !failed;
}

#define STREQUALTEST(x, y) std::cout << "==(\"" << x << "\",\"" << y << "\") " << ((testpassed = (x == y)) ? "SUCCESS" : "FAILURE") << std::endl

template<typename T>
static bool DoStreamTest(const char* expected[], T& stream)
{
	std::string token;
	bool tooMany = false, failed = false, testpassed;
	size_t i;
	for(i = 0; stream.GetToken(token); ++i)
	{
		if(tooMany || !expected[i])
		{
			tooMany = failed = true;
			std::cout << "FAILURE: Received extra token \"" << token << "\"" << std::endl;
		}
		else
		{
			STREQUALTEST(token, expected[i]);
			failed = !testpassed || failed;
		}
	}
	if(!tooMany)
		while(expected[i])
		{
			failed = true;
			std::cout << "FAILURE: Did not receive expected token \"" << expected[i++] << "\"" << std::endl;
		}
	return !failed;
}

#define STREAMTEST(a,b,...) \
do { \
	a stream b; \
	static const char* expected[] = __VA_ARGS__; \
	failed = !DoStreamTest(expected, stream) || failed; \
	std::cout << std::endl; \
} while(0)

static bool DoCommaSepStreamTests()
{
	std::cout << "Comma sepstream tests" << std::endl << std::endl;
	bool failed = false;

	STREAMTEST(irc::commasepstream, ("this,is,a,comma,stream", false), { "this", "is", "a", "comma", "stream", NULL });
	STREAMTEST(irc::commasepstream, ("with,lots,,of,,,commas", false), { "with", "lots", "", "of", "", "", "commas", NULL });
	STREAMTEST(irc::commasepstream, (",comma,at,the,beginning", false), { "", "comma", "at", "the", "beginning", NULL });
	STREAMTEST(irc::commasepstream, ("commas,at,the,end,,", false), { "commas", "at", "the", "end", "", "", NULL });
	STREAMTEST(irc::commasepstream, (",", false), { "", "", NULL });
	STREAMTEST(irc::commasepstream, ("", false), { "", NULL });

	std::cout << "Result of comma sepstream tests:";
	COUTFAILED();
	return !failed;
}

static bool DoSpaceSepStreamTests()
{
	std::cout << "Space sepstream tests" << std::endl << std::endl;
	bool failed = false;

	STREAMTEST(irc::spacesepstream, ("this is a space stream", true), { "this", "is", "a", "space", "stream", NULL });
	STREAMTEST(irc::spacesepstream, ("with lots  of   spaces", true), { "with", "lots", "of", "spaces", NULL });
	STREAMTEST(irc::spacesepstream, (" space at the beginning", true), { "space", "at", "the", "beginning", NULL });
	STREAMTEST(irc::spacesepstream, ("spaces at the end  ", true), { "spaces", "at", "the", "end", NULL });
	STREAMTEST(irc::spacesepstream, (" ", true), { NULL });
	STREAMTEST(irc::spacesepstream, ("", true), { NULL });

	std::cout << "Result of space sepstream tests:";
	COUTFAILED();
	return !failed;
}

static bool DoTokenStreamTests()
{
	std::cout << "Token stream tests" << std::endl << std::endl;
	bool failed = false;

	STREAMTEST(irc::tokenstream, ("just some words and spaces"), { "just", "some", "words", "and", "spaces", NULL });
	STREAMTEST(irc::tokenstream, (":not actually all one token"), { ":not", "actually", "all", "one", "token", NULL });
	STREAMTEST(irc::tokenstream, ("several small tokens :and one large one"), { "several", "small", "tokens", "and one large one", NULL });
	STREAMTEST(irc::tokenstream, ("with 3 tokens "), { "with", "3", "tokens", NULL });
	STREAMTEST(irc::tokenstream, ("with a blank token at the end :"), { "with", "a", "blank", "token", "at", "the", "end", "", NULL });
	STREAMTEST(irc::tokenstream, ("with a space at the end : "), { "with", "a", "space", "at", "the", "end", " ", NULL });
	STREAMTEST(irc::tokenstream, ("a :large token ending in a colon:"), { "a", "large token ending in a colon:", NULL });
	STREAMTEST(irc::tokenstream, ("several tokens with the last ending in a colon:"), { "several", "tokens", "with", "the", "last", "ending", "in", "a", "colon:", NULL });

	std::cout << "Result of token stream tests:";
	COUTFAILED();
	return !failed;
}

TestSuite::TestSuite()
{
	std::cout << std::endl << "*** STARTING TESTSUITE ***" << std::endl;

	std::string modname;
	char choice;
	bool failed;

	while (1)
	{
		std::cout << "(1) Call all module RunTestSuite() methods" << std::endl;
		std::cout << "(2) Run all core tests" << std::endl;

		std::cout << std::endl << "(3) Run wildcard and CIDR tests" << std::endl;
		std::cout << "(4) Run comma sepstream tests" << std::endl;
		std::cout << "(5) Run space sepstream tests" << std::endl;
		std::cout << "(6) Run token stream tests" << std::endl;

		std::cout << std::endl << "(L) Load a module" << std::endl;
		std::cout << "(U) Unload a module" << std::endl;
		std::cout << "(X) Exit test suite" << std::endl;

		std::cout << std::endl << "Choices (Enter one or more options as a list then press enter, e.g. 15X): ";
		std::cin >> choice;

		std::cout << std::endl;

		switch (choice)
		{
			case '1':
				for(std::map<std::string, Module*>::const_iterator i = ServerInstance->Modules->GetModules().begin(); i != ServerInstance->Modules->GetModules().end(); i++)
				{
					Module* m = i->second;
					try
					{
						m->RunTestSuite();
					}
					catch (CoreException& e)
					{
						std::cout << "Module " << m->ModuleSourceFile << " failed: " << e.err;
					}
				}
				break;

			case '2':
				failed = false;
				failed = !DoWildTests() || failed;
				failed = !DoCommaSepStreamTests() || failed;
				failed = !DoSpaceSepStreamTests() || failed;
				failed = !DoTokenStreamTests() || failed;
				std::cout << "Final result of all tests:";
				COUTFAILED();
				break;

			case '3':
				DoWildTests();
				break;

			case '4':
				DoCommaSepStreamTests();
				break;

			case '5':
				DoSpaceSepStreamTests();
				break;

			case '6':
				DoTokenStreamTests();
				break;

			case 'L':
				std::cout << "Enter module filename to load: ";
				std::cin >> modname;
				std::cout << std::endl << (ServerInstance->Modules->Load(modname.c_str()) ? "SUCCESS" : "FAILURE") << std::endl;
				break;

			case 'U':
				std::cout << "Enter module filename to unload: ";
				std::cin >> modname;
				{
					Module* m = ServerInstance->Modules->Find(modname);
					std::cout << std::endl << (ServerInstance->Modules->Unload(m) ? "SUCCESS" : "FAILURE") << std::endl;
					ServerInstance->AtomicActions->Run();
				}
				break;

			case 'X':
				return;

			default:
				std::cout << "Invalid option" << std::endl;
				break;
		}
	}
}

TestSuite::~TestSuite()
{
	std::cout << std::endl << "*** END OF TEST SUITE ***" << std::endl;
}

