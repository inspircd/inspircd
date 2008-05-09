/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2008 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *          the file COPYING for details.
 *
 * ---------------------------------------------------
 */

/* $Core: libIRCDtestsuite */

#include "inspircd.h"
#include "testsuite.h"
#include "threadengine.h"
#include "wildcard.h"
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

		cout << endl << "(X) Exit test suite\n";

		cout << "\nChoice: ";
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
#define WCTEST(x, y) cout << "match(\"" << x << "\",\"" << y "\") " << ((passed = (match(x, y) || passed)) ? " SUCCESS!\n" : " FAILURE\n")
/* Test that x does not match y with match() */
#define WCTESTNOT(x, y) cout << "!match(\"" << x << "\",\"" << y "\") " << ((passed = ((!match(x, y)) || passed)) ? " SUCCESS!\n" : " FAILURE\n")

/* Test that x matches y with match() and cidr enabled */
#define CIDRTEST(x, y) cout << "match(\"" << x << "\",\"" << y "\", true) " << ((passed = (match(x, y, true) || passed)) ? " SUCCESS!\n" : " FAILURE\n")
/* Test that x does not match y with match() and cidr enabled */
#define CIDRTESTNOT(x, y) cout << "!match(\"" << x << "\",\"" << y "\", true) " << ((passed = ((!match(x, y, true)) || passed)) ? " SUCCESS!\n" : " FAILURE\n")

bool TestSuite::DoWildTests()
{
	cout << "\n\nWildcard and CIDR tests\n\n";
	bool passed = false;

	WCTEST("foobar", "*");
	WCTEST("foobar", "foo*");
	WCTEST("foobar", "*bar");
	WCTEST("foobar", "foo??r");

	WCTESTNOT("foobar", "bazqux");
	WCTESTNOT("foobar", "*qux");
	WCTESTNOT("foobar", "foo*x");
	WCTESTNOT("foobar", "baz*");
	WCTESTNOT("foobar", "foo???r");
	WCTESTNOT("foobar", "");

	CIDRTEST("brain@1.2.3.4", "*@1.2.0.0/16");
	CIDRTEST("brain@1.2.3.4", "*@1.2.3.0/24");

	CIDRTESTNOT("brain@1.2.3.4", "x*@1.2.0.0/16");
	CIDRTESTNOT("brain@1.2.3.4", "*@1.3.4.0/24");

	CIDRTESTNOT("brain@1.2.3.4", "*@/24");
	CIDRTESTNOT("brain@1.2.3.4", "@1.2.3.4/9");
	CIDRTESTNOT("brain@1.2.3.4", "@");
	CIDRTESTNOT("brain@1.2.3.4", "");

	return passed;
}

bool TestSuite::DoThreadTests()
{
	std::string anything;
	ThreadEngine* te = NULL;

	cout << "Creating new ThreadEngine class...\n";
	try
	{
		ThreadEngineFactory* tef = new ThreadEngineFactory();
		te = tef->Create(ServerInstance);
		delete tef;
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
			te->Create(tst);
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

