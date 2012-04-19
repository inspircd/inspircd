/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2012 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef SASL_H
#define SASL_H

class SASLFallback : public Event
{
 public:
	const parameterlist& params;
	SASLFallback(Module* me, const parameterlist& p)
		: Event(me, "sasl_fallback"), params(p)
	{
		Send();
	}
};

#endif
