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
#include <iostream>

using namespace std;

TestSuite::TestSuite(InspIRCd* Instance) : ServerInstance(Instance)
{
	cout << "\n\n*** STARTING TESTSUITE ***\n";

	std::string modname;

	while (1)
	{
		cout << "(1) Call all module OnRunTestSuite() methods\n";
		cout << "(2) Load a module\n";
		cout << "(3) Unload a module\n";
		cout << "(4) Threading tests\n";
	
		switch (fgetc(stdin))
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
		}
		cout << endl;
	}
}

bool TestSuite::DoThreadTests()
{
	return true;
}

TestSuite::~TestSuite()
{
	cout << "\n\n*** END OF TEST SUITE ***\n";
}

