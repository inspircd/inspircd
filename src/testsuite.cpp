/*	   +------------------------------------+
 *	   | Inspire Internet Relay Chat Daemon |
 *	   +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2012 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *		  the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/* $Core */

#include "inspircd.h"
#include "testsuite.h"
#include "threadengine.h"
#include <iostream>

using namespace std;

class TestSuiteThread : public Thread
{
 public:
	TestSuiteThread() : Thread()
	{
	}

	virtual ~TestSuiteThread()
	{
	}

	virtual void Run()
	{
		while (GetExitFlag() == false)
		{
			cout << "Test suite thread run...\n";
			sleep(5);
		}
	}
};

TestSuite::TestSuite(InspIRCd* Instance) : ServerInstance(Instance)
{
	cout << "\n\n*** STARTING TESTSUITE ***\n";

	std::string modname;
	char choice;

	while (1)
	{
		cout << "(1) Call all module OnRunTestSuite() methods\n";
		cout << "(2) Load a module\n";
		cout << "(3) Unload a module\n";
		cout << "(4) Threading tests\n";
		cout << "(5) Wildcard and CIDR tests\n";
		cout << "(6) Comma sepstream tests\n";
		cout << "(7) Space sepstream tests\n";

		cout << endl << "(X) Exit test suite\n";

		cout << "\nChoices (Enter one or more options as a list then press enter, e.g. 15X): ";
		cin >> choice;

		if (!choice)
			continue;

		switch (choice)
		{
			case '1':
				FOREACH_MOD(I_OnRunTestSuite, OnRunTestSuite());
				break;
			case '2':
				cout << "Enter module filename to load: ";
				cin >> modname;
				cout << (Instance->Modules->Load(modname.c_str()) ? "\nSUCCESS!\n" : "\nFAILURE\n");
				break;
			case '3':
				cout << "Enter module filename to unload: ";
				cin >> modname;
				cout << (Instance->Modules->Unload(modname.c_str()) ? "\nSUCCESS!\n" : "\nFAILURE\n");
				break;
			case '4':
				cout << (DoThreadTests() ? "\nSUCCESS!\n" : "\nFAILURE\n");
				break;
			case '5':
				cout << (DoWildTests() ? "\nSUCCESS!\n" : "\nFAILURE\n");
				break;
			case '6':
				cout << (DoCommaSepStreamTests() ? "\nSUCCESS!\n" : "\nFAILURE\n");
				break;
			case '7':
				cout << (DoSpaceSepStreamTests() ? "\nSUCCESS!\n" : "\nFAILURE\n");
				break;
			case 'X':
				return;
				break;
			default:
				cout << "Invalid option\n";
				break;
		}
		cout << endl;
	}
}

/* Test that x matches y with match() */
#define WCTEST(x, y) cout << "match(\"" << x << "\",\"" << y "\") " << ((passed = (InspIRCd::Match(x, y, NULL))) ? " SUCCESS!\n" : " FAILURE\n")
/* Test that x does not match y with match() */
#define WCTESTNOT(x, y) cout << "!match(\"" << x << "\",\"" << y "\") " << ((passed = ((!InspIRCd::Match(x, y, NULL)))) ? " SUCCESS!\n" : " FAILURE\n")

/* Test that x matches y with match() and cidr enabled */
#define CIDRTEST(x, y) cout << "match(\"" << x << "\",\"" << y "\", true) " << ((passed = (InspIRCd::MatchCIDR(x, y, NULL))) ? " SUCCESS!\n" : " FAILURE\n")
/* Test that x does not match y with match() and cidr enabled */
#define CIDRTESTNOT(x, y) cout << "!match(\"" << x << "\",\"" << y "\", true) " << ((passed = ((!InspIRCd::MatchCIDR(x, y, NULL)))) ? " SUCCESS!\n" : " FAILURE\n")

bool TestSuite::DoWildTests()
{
	cout << "\n\nWildcard and CIDR tests\n\n";
	bool passed = false;

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

	CIDRTESTNOT("brain@1.2.3.4", "x*@1.2.0.0/16");
	CIDRTESTNOT("brain@1.2.3.4", "*@1.3.4.0/24");
	CIDRTESTNOT("1.2.3.4", "1.2.4.0/24");
	CIDRTESTNOT("brain@1.2.3.4", "*@/24");
	CIDRTESTNOT("brain@1.2.3.4", "@1.2.3.4/9");
	CIDRTESTNOT("brain@1.2.3.4", "@");
	CIDRTESTNOT("brain@1.2.3.4", "");

	return true;
}


#define STREQUALTEST(x, y) cout << "==(\"" << x << ",\"" << y "\") " << ((passed = (x == y)) ? "SUCCESS\n" : "FAILURE\n")

bool TestSuite::DoCommaSepStreamTests()
{
	bool passed = false;
	irc::commasepstream items("this,is,a,comma,stream");
	std::string item;
	int idx = 0;

	while (items.GetToken(item))
	{
		idx++;

		switch (idx)
		{
			case 1:
				STREQUALTEST(item, "this");
				break;
			case 2:
				STREQUALTEST(item, "is");
				break;
			case 3:
				STREQUALTEST(item, "a");
				break;
			case 4:
				STREQUALTEST(item, "comma");
				break;
			case 5:
				STREQUALTEST(item, "stream");
				break;
			default:
				cout << "COMMASEPSTREAM: FAILURE: Got an index too many! " << idx << " items\n";
				break;
		}
	}

	return true;
}

bool TestSuite::DoSpaceSepStreamTests()
{
	bool passed = false;

	irc::spacesepstream list("this is a space stream");
	std::string item;
	int idx = 0;

	while (list.GetToken(item))
	{
		idx++;

		switch (idx)
		{
			case 1:
				STREQUALTEST(item, "this");
				break;
			case 2:
				STREQUALTEST(item, "is");
				break;
			case 3:
				STREQUALTEST(item, "a");
				break;
			case 4:
				STREQUALTEST(item, "space");
				break;
			case 5:
				STREQUALTEST(item, "stream");
				break;
			default:
				cout << "SPACESEPSTREAM: FAILURE: Got an index too many! " << idx << " items\n";
				break;
		}
	}
	return true;
}

bool TestSuite::DoThreadTests()
{
	std::string anything;
	ThreadEngine* te = NULL;

	cout << "Creating new ThreadEngine class...\n";
	try
	{
		te = new ThreadEngine(ServerInstance);
	}
	catch (...)
	{
		cout << "Creation failed, test failure.\n";
		return false;
	}
	cout << "Creation success, type " << te->GetName() << "\n";

	cout << "Allocate: new TestSuiteThread...\n";
	TestSuiteThread* tst = new TestSuiteThread();

	cout << "ThreadEngine::Create on TestSuiteThread...\n";
	try
	{
		try
		{
			te->Start(tst);
		}
		catch (CoreException &ce)
		{
			cout << "Failure: " << ce.GetReason() << endl;
		}
	}
	catch (...)
	{
		cout << "Failure, unhandled exception\n";
	}

	cout << "Type any line and press enter to end test.\n";
	cin >> anything;

	/* Thread engine auto frees thread on delete */
	cout << "Waiting for thread to exit... " << flush;
	delete tst;
	cout << "Done!\n";

	cout << "Delete ThreadEngine... ";
	delete te;
	cout << "Done!\n";

	return true;
}

TestSuite::~TestSuite()
{
	cout << "\n\n*** END OF TEST SUITE ***\n";
}

