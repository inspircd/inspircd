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

TestSuite::TestSuite(InspIRCd* ServerInstance)
{
	printf("\n\n*** STARTING TESTSUITE ***\n");
	FOREACH_MOD(I_OnRunTestSuite, OnRunTestSuite());
}

TestSuite::~TestSuite()
{
	printf("\n\n*** END OF TEST SUITE ***\n");
}

