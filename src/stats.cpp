/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *	    the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include "xline.h"
#include "commands/cmd_whowas.h"

void InspIRCd::DoStats(char statschar, User* user, string_list &results)
{
	std::string sn(this->Config->ServerName);

	if (!user->HasPrivPermission("servers/auspex") && Config->UserStats.find(statschar) == std::string::npos)
	{
		this->SNO->WriteToSnoMask('t',
				"%s '%c' denied for %s (%s@%s)",
				(IS_LOCAL(user) ? "Stats" : "Remote stats"),
				statschar, user->nick.c_str(), user->ident.c_str(), user->host.c_str());
		results.push_back(sn + " 481 " + user->nick + " :Permission denied - STATS " + statschar + " requires the servers/auspex priv.");
		return;
	}

	ModResult MOD_RESULT;
	FIRST_MOD_RESULT(OnStats, MOD_RESULT, (statschar, user, results));
	if (MOD_RESULT == MOD_RES_DENY)
	{
		results.push_back(sn+" 219 "+user->nick+" "+statschar+" :End of /STATS report");
		this->SNO->WriteToSnoMask('t',"%s '%c' requested by %s (%s@%s)",
			(IS_LOCAL(user) ? "Stats" : "Remote stats"), statschar, user->nick.c_str(), user->ident.c_str(), user->host.c_str());
		return;
	}

	switch (statschar)
	{
		/* stats p (show listening ports and registered clients on each) */
		case 'p':
		{
			for (size_t i = 0; i < this->ports.size(); i++)
			{
				std::string ip = this->ports[i]->bind_addr;
				if (ip.empty())
					ip.assign("*");
				std::string type = ports[i]->bind_tag->getString("type", "clients");
				std::string hook = ports[i]->bind_tag->getString("ssl", "plaintext");

				results.push_back(sn+" 249 "+user->nick+" :"+ ip + ":"+ConvToStr(ports[i]->bind_port)+
					" (" + type + ", " + hook + ")");
			}
		}
		break;

		/* These stats symbols must be handled by a linking module */
		case 'n':
		case 'c':
		break;

		case 'i':
		{
			for (ClassVector::iterator i = this->Config->Classes.begin(); i != this->Config->Classes.end(); i++)
			{
				ConnectClass* c = *i;
				std::stringstream res;
				res << sn << " 215 " << user->nick << " I " << c->name << ' ';
				if (c->type == CC_ALLOW)
					res << '+';
				if (c->type == CC_DENY)
					res << '-';

				if (c->type == CC_NAMED)
					res << '*';
				else
					res << c->host;

				if (c->port)
					res << ' ' << c->port << ' ';
				else
					res << " * ";

				res << c->GetRecvqMax() << ' ' << c->GetSendqSoftMax() << ' ' << c->GetSendqHardMax()
					<< ' ' << c->GetCommandRate() << ' ' << c->GetPenaltyThreshold();
				if (c->fakelag)
					res << '*';
				results.push_back(res.str());
			}
		}
		break;

		case 'Y':
		{
			int idx = 0;
			for (ClassVector::iterator i = this->Config->Classes.begin(); i != this->Config->Classes.end(); i++)
			{
				ConnectClass* c = *i;
				results.push_back(sn+" 215 "+user->nick+" i NOMATCH * "+c->GetHost()+" "+ConvToStr(c->limit ? c->limit : this->SE->GetMaxFds())+" "+ConvToStr(idx)+" "+this->Config->ServerName+" *");
				results.push_back(sn+" 218 "+user->nick+" Y "+ConvToStr(idx)+" "+ConvToStr(c->GetPingTime())+" 0 "+ConvToStr(c->GetSendqHardMax())+" :"+
						ConvToStr(c->GetRecvqMax())+" "+ConvToStr(c->GetRegTimeout()));
				idx++;
			}
		}
		break;

		case 'U':
		{
			for(std::map<irc::string, bool>::iterator i = Config->ulines.begin(); i != Config->ulines.end(); ++i)
			{
				results.push_back(sn+" 248 "+user->nick+" U "+std::string(i->first.c_str()));
			}
		}
		break;

		case 'P':
		{
			int idx = 0;
			for (user_hash::iterator i = this->Users->clientlist->begin(); i != this->Users->clientlist->end(); i++)
			{
				if (IS_OPER(i->second) && !this->ULine(i->second->server))
				{
					results.push_back(sn+" 249 "+user->nick+" :"+i->second->nick+" ("+i->second->ident+"@"+i->second->dhost+") Idle: "+
							(IS_LOCAL(i->second) ? ConvToStr(this->Time() - i->second->idle_lastmsg) + " secs" : "unavailable"));
					idx++;
				}
			}
			results.push_back(sn+" 249 "+user->nick+" :"+ConvToStr(idx)+" OPER(s)");
		}
		break;

		case 'k':
			this->XLines->InvokeStats("K",216,user,results);
		break;
		case 'g':
			this->XLines->InvokeStats("G",223,user,results);
		break;
		case 'q':
			this->XLines->InvokeStats("Q",217,user,results);
		break;
		case 'Z':
			this->XLines->InvokeStats("Z",223,user,results);
		break;
		case 'e':
			this->XLines->InvokeStats("E",223,user,results);
		break;
		case 'E':
			results.push_back(sn+" 249 "+user->nick+" :Total events: "+ConvToStr(this->SE->TotalEvents));
			results.push_back(sn+" 249 "+user->nick+" :Read events:  "+ConvToStr(this->SE->ReadEvents));
			results.push_back(sn+" 249 "+user->nick+" :Write events: "+ConvToStr(this->SE->WriteEvents));
			results.push_back(sn+" 249 "+user->nick+" :Error events: "+ConvToStr(this->SE->ErrorEvents));
		break;

		/* stats m (list number of times each command has been used, plus bytecount) */
		case 'm':
			for (Commandtable::iterator i = this->Parser->cmdlist.begin(); i != this->Parser->cmdlist.end(); i++)
			{
				if (i->second->use_count)
				{
					/* RPL_STATSCOMMANDS */
					results.push_back(sn+" 212 "+user->nick+" "+i->second->name+" "+ConvToStr(i->second->use_count)+" "+ConvToStr(i->second->total_bytes));
				}
			}
		break;

		/* stats z (debug and memory info) */
		case 'z':
		{
			results.push_back(sn+" 249 "+user->nick+" :Users: "+ConvToStr(this->Users->clientlist->size()));
			results.push_back(sn+" 249 "+user->nick+" :Channels: "+ConvToStr(this->chanlist->size()));
			results.push_back(sn+" 249 "+user->nick+" :Commands: "+ConvToStr(this->Parser->cmdlist.size()));

			if (!this->Config->WhoWasGroupSize == 0 && !this->Config->WhoWasMaxGroups == 0)
			{
				Module* whowas = Modules->Find("cmd_whowas.so");
				if (whowas)
				{
					WhowasRequest req(NULL, whowas, WhowasRequest::WHOWAS_STATS);
					req.user = user;
					req.Send();
					results.push_back(sn+" 249 "+user->nick+" :"+req.value);
				}
			}

			float kbitpersec_in, kbitpersec_out, kbitpersec_total;
			char kbitpersec_in_s[30], kbitpersec_out_s[30], kbitpersec_total_s[30];

			this->SE->GetStats(kbitpersec_in, kbitpersec_out, kbitpersec_total);

			snprintf(kbitpersec_total_s, 30, "%03.5f", kbitpersec_total);
			snprintf(kbitpersec_out_s, 30, "%03.5f", kbitpersec_out);
			snprintf(kbitpersec_in_s, 30, "%03.5f", kbitpersec_in);

			results.push_back(sn+" 249 "+user->nick+" :Bandwidth total:  "+ConvToStr(kbitpersec_total_s)+" kilobits/sec");
			results.push_back(sn+" 249 "+user->nick+" :Bandwidth out:    "+ConvToStr(kbitpersec_out_s)+" kilobits/sec");
			results.push_back(sn+" 249 "+user->nick+" :Bandwidth in:     "+ConvToStr(kbitpersec_in_s)+" kilobits/sec");

#ifndef WIN32
			/* Moved this down here so all the not-windows stuff (look w00tie, I didn't say win32!) is in one ifndef.
			 * Also cuts out some identical code in both branches of the ifndef. -- Om
			 */
			rusage R;

			/* Not sure why we were doing '0' with a RUSAGE_SELF comment rather than just using RUSAGE_SELF -- Om */
			if (!getrusage(RUSAGE_SELF,&R))	/* RUSAGE_SELF */
			{
				results.push_back(sn+" 249 "+user->nick+" :Total allocation: "+ConvToStr(R.ru_maxrss)+"K");
				results.push_back(sn+" 249 "+user->nick+" :Signals:          "+ConvToStr(R.ru_nsignals));
				results.push_back(sn+" 249 "+user->nick+" :Page faults:      "+ConvToStr(R.ru_majflt));
				results.push_back(sn+" 249 "+user->nick+" :Swaps:            "+ConvToStr(R.ru_nswap));
				results.push_back(sn+" 249 "+user->nick+" :Context Switches: Voluntary; "+ConvToStr(R.ru_nvcsw)+" Involuntary; "+ConvToStr(R.ru_nivcsw));

				char percent[30];

				float n_elapsed = (ServerInstance->Time() - this->stats->LastSampled.tv_sec) * 1000000
					+ (ServerInstance->Time_ns() - this->stats->LastSampled.tv_nsec) / 1000;
				float n_eaten = ((R.ru_utime.tv_sec - this->stats->LastCPU.tv_sec) * 1000000 + R.ru_utime.tv_usec - this->stats->LastCPU.tv_usec);
				float per = (n_eaten / n_elapsed) * 100;

				snprintf(percent, 30, "%03.5f%%", per);
				results.push_back(sn+" 249 "+user->nick+" :CPU Use (now):    "+percent);

				n_elapsed = ServerInstance->Time() - ServerInstance->startup_time;
				n_eaten = (float)R.ru_utime.tv_sec + R.ru_utime.tv_usec / 100000.0;
				per = (n_eaten / n_elapsed) * 100;
				snprintf(percent, 30, "%03.5f%%", per);
				results.push_back(sn+" 249 "+user->nick+" :CPU Use (total):  "+percent);
			}
#else
			PROCESS_MEMORY_COUNTERS MemCounters;
			if (GetProcessMemoryInfo(GetCurrentProcess(), &MemCounters, sizeof(MemCounters)))
			{
				results.push_back(sn+" 249 "+user->nick+" :Total allocation: "+ConvToStr((MemCounters.WorkingSetSize + MemCounters.PagefileUsage) / 1024)+"K");
				results.push_back(sn+" 249 "+user->nick+" :Pagefile usage:   "+ConvToStr(MemCounters.PagefileUsage / 1024)+"K");
				results.push_back(sn+" 249 "+user->nick+" :Page faults:      "+ConvToStr(MemCounters.PageFaultCount));
				results.push_back(sn+" 249 "+user->nick+" :CPU Usage: " + ConvToStr(getcpu()) + "%");
			}
#endif
		}
		break;

		case 'T':
		{
			char buffer[MAXBUF];
			results.push_back(sn+" 249 "+user->nick+" :accepts "+ConvToStr(this->stats->statsAccept)+" refused "+ConvToStr(this->stats->statsRefused));
			results.push_back(sn+" 249 "+user->nick+" :unknown commands "+ConvToStr(this->stats->statsUnknown));
			results.push_back(sn+" 249 "+user->nick+" :nick collisions "+ConvToStr(this->stats->statsCollisions));
			results.push_back(sn+" 249 "+user->nick+" :dns requests "+ConvToStr(this->stats->statsDnsGood+this->stats->statsDnsBad)+" succeeded "+ConvToStr(this->stats->statsDnsGood)+" failed "+ConvToStr(this->stats->statsDnsBad));
			results.push_back(sn+" 249 "+user->nick+" :connection count "+ConvToStr(this->stats->statsConnects));
			snprintf(buffer,MAXBUF," 249 %s :bytes sent %5.2fK recv %5.2fK",user->nick.c_str(),this->stats->statsSent / 1024,this->stats->statsRecv / 1024);
			results.push_back(sn+buffer);
		}
		break;

		/* stats o */
		case 'o':
		{
			ConfigTagList tags = ServerInstance->Config->ConfTags("oper");
			for(ConfigIter i = tags.first; i != tags.second; ++i)
			{
				ConfigTag* tag = i->second;
				results.push_back(sn+" 243 "+user->nick+" O "+tag->getString("host")+" * "+
					tag->getString("name") + " " + tag->getString("type")+" 0");
			}
		}
		break;
		case 'O':
		{
			for(OperIndex::iterator i = ServerInstance->Config->oper_blocks.begin(); i != ServerInstance->Config->oper_blocks.end(); i++)
			{
				// just the types, not the actual oper blocks...
				if (i->first[0] != ' ')
					continue;
				OperInfo* tag = i->second;
				tag->init();
				std::string umodes;
				std::string cmodes;
				for(char c='A'; c < 'z'; c++)
				{
					ModeHandler* mh = ServerInstance->Modes->FindMode(c, MODETYPE_USER);
					if (mh && mh->NeedsOper() && tag->AllowedUserModes[c])
						umodes.push_back(c);
					mh = ServerInstance->Modes->FindMode(c, MODETYPE_CHANNEL);
					if (mh && mh->NeedsOper() && tag->AllowedChanModes[c])
						cmodes.push_back(c);
				}
				results.push_back(sn+" 243 "+user->nick+" O "+tag->NameStr() + " " + umodes + " " + cmodes);
			}
		}
		break;

		/* stats l (show user I/O stats) */
		case 'l':
			results.push_back(sn+" 211 "+user->nick+" :nick[ident@host] sendq cmds_out bytes_out cmds_in bytes_in time_open");
			for (std::vector<LocalUser*>::iterator n = this->Users->local_users.begin(); n != this->Users->local_users.end(); n++)
			{
				LocalUser* i = *n;
				results.push_back(sn+" 211 "+user->nick+" "+i->nick+"["+i->ident+"@"+i->dhost+"] "+ConvToStr(i->eh.getSendQSize())+" "+ConvToStr(i->cmds_out)+" "+ConvToStr(i->bytes_out)+" "+ConvToStr(i->cmds_in)+" "+ConvToStr(i->bytes_in)+" "+ConvToStr(this->Time() - i->age));
			}
		break;

	/* stats L (show user I/O stats with IP addresses) */
		case 'L':
			results.push_back(sn+" 211 "+user->nick+" :nick[ident@ip] sendq cmds_out bytes_out cmds_in bytes_in time_open");
			for (std::vector<LocalUser*>::iterator n = this->Users->local_users.begin(); n != this->Users->local_users.end(); n++)
			{
				LocalUser* i = *n;
				results.push_back(sn+" 211 "+user->nick+" "+i->nick+"["+i->ident+"@"+i->GetIPString()+"] "+ConvToStr(i->eh.getSendQSize())+" "+ConvToStr(i->cmds_out)+" "+ConvToStr(i->bytes_out)+" "+ConvToStr(i->cmds_in)+" "+ConvToStr(i->bytes_in)+" "+ConvToStr(this->Time() - i->age));
			}
		break;

		/* stats u (show server uptime) */
		case 'u':
		{
			time_t current_time = 0;
			current_time = this->Time();
			time_t server_uptime = current_time - this->startup_time;
			struct tm* stime;
			stime = gmtime(&server_uptime);
			/* i dont know who the hell would have an ircd running for over a year nonstop, but
			 * Craig suggested this, and it seemed a good idea so in it went */
			if (stime->tm_year > 70)
			{
				char buffer[MAXBUF];
				snprintf(buffer,MAXBUF," 242 %s :Server up %d years, %d days, %.2d:%.2d:%.2d",user->nick.c_str(),(stime->tm_year-70),stime->tm_yday,stime->tm_hour,stime->tm_min,stime->tm_sec);
				results.push_back(sn+buffer);
			}
			else
			{
				char buffer[MAXBUF];
				snprintf(buffer,MAXBUF," 242 %s :Server up %d days, %.2d:%.2d:%.2d",user->nick.c_str(),stime->tm_yday,stime->tm_hour,stime->tm_min,stime->tm_sec);
				results.push_back(sn+buffer);
			}
		}
		break;

		default:
		break;
	}

	results.push_back(sn+" 219 "+user->nick+" "+statschar+" :End of /STATS report");
	this->SNO->WriteToSnoMask('t',"%s '%c' requested by %s (%s@%s)",
		(IS_LOCAL(user) ? "Stats" : "Remote stats"), statschar, user->nick.c_str(), user->ident.c_str(), user->host.c_str());
	return;
}
