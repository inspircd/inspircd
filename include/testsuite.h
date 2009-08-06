/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *          the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef __TESTSUITE_H__
#define __TESTSUITE_H__

class InspIRCd;

class TestSuite : public Extensible
{
 private:
	InspIRCd* ServerInstance;
 public:
	TestSuite(InspIRCd* Instance);
	~TestSuite();

	bool DoThreadTests();
	bool DoWildTests();
	bool DoCommaSepStreamTests();
	bool DoSpaceSepStreamTests();
};

#endif
