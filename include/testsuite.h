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
};

#endif
