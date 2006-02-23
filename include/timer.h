/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd is copyright (C) 2002-2006 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *                <Craig@chatspike.net>
 *
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

class InspTimer
{
 private:
	time_t trigger;
 public:
	InspTimer(long secs_from_now,time_t now)
	{
		trigger = now + secs_from_now;
	}
	virtual ~InspTimer() { }
	virtual time_t GetTimer()
	{
		return trigger;
	}
	virtual void Tick(time_t TIME)
	{
	}
};

void TickTimers(time_t TIME);
void AddTimer(InspTimer* T);

