/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2010 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */


#include "inspircd.h"

/******************************************************************************
 * Flat-file database read/write
 ******************************************************************************/

/* structure for single entry */
struct EntryDescriptor
{
	std::string name, ts, topicset, topicsetby, topic, modes;
};
/** reads the channel database and returns structure with it */
class DatabaseReader
{
	/* the entry descriptor */
	EntryDescriptor entry;
	/* file */
	FILE *fd;
 public:
	/* constructor, opens the file */
	DatabaseReader (std::string filename)
	{
		/* initialize */
		fd = NULL;
		/* open the file */
		fd = fopen (filename.c_str ( ), "r");
		/* if we can't open the file, return. */
		if (!fd) return;
	}
	/* destructor will close the file */
	~DatabaseReader ( )
	{
		/* if fd is not null, close it */
		if (fd) fclose (fd);
	}
	/* get next entry */
	EntryDescriptor *next ( )
	{
		/* if fd is NULL, fake eof */
		if (!fd) return 0;
		/* clear data */
		entry.name = "";
		entry.ts = "";
		entry.topicset = "";
		entry.topicsetby = "";
		entry.modes = "";
		entry.topic = "";
		std::string str;
		/* read single characters from the file and add them to the string, end at EOF or \n, ignore \r */
		while (1)
		{
			int c = fgetc (fd);
			if ((c == EOF) || ((unsigned char)c == '\n')) break;
			if ((unsigned char)c == '\r') continue;
			str.push_back ((unsigned char)c);
		}
		/* ready to parse the line */
		if (str == "") return 0;
		std::string token;
		irc::spacesepstream sep(str);
		/* get first one */
		/* malformed if it is not chaninfo */
		if (!sep.GetToken (token) || token != "chaninfo")
		{
			ServerInstance->Logs->Log ("MODULE", DEFAULT, "malformed channel database");
			return 0;
		}
		/* okay, read channel name */
		/* name was get, but if it's the last token, database is malformed */
		if (!sep.GetToken (token))
		{
			ServerInstance->Logs->Log ("MODULE", DEFAULT, "malformed channel database");
			return 0;
		}
		/* save name */
		entry.name = token;
		/* set channel timestamp */
		sep.GetToken (entry.ts);
		/* initial entry read, read next lines in a loop until end, eof means malformed database again */
		while (1)
		{
			str.clear ( );
			/* read the line */
			while (1)
			{
				int c = fgetc (fd);
				if (c == EOF)
				{
					ServerInstance->Logs->Log ("MODULE", DEFAULT, "malformed channel database");
					return 0;
				}
					unsigned char c2 = (unsigned char)c;
				if (c2 == '\n') break;
				if (c2 == '\r') continue;
				str.push_back (c2);
			}
			irc::spacesepstream sep2(str);
			/* get the token */
			if (str == "")
			{
				ServerInstance->Logs->Log ("MODULE", DEFAULT, "malformed channel database");
				return 0;
			}
			sep2.GetToken (token);
			/* break the loop if token is "end" */
			if (token == "end") break;
			/* it is not, so the large if statement there */
			else if (token == "topic")
			{
				/* topic info, if end then it is malformed */
				if (sep2.StreamEnd ( ))
				{
					ServerInstance->Logs->Log ("MODULE", DEFAULT, "malformed topic declaration in channel database for channel %s", entry.name.c_str ( ));
				}
				/* if not, then read topic set time */
				/* if end then malformed */
				if (!sep2.GetToken (token))
				{
					ServerInstance->Logs->Log ("MODULE", DEFAULT, "malformed topic declaration in channel database for channel %s", entry.name.c_str ( ));
				}
				/* save that */
				entry.topicset = token;
				/* get next token */
				/* if last then malformed */
				if (!sep2.GetToken (token))
				{
					ServerInstance->Logs->Log ("MODULE", DEFAULT, "malformed topic declaration in channel database for channel %s", entry.name.c_str ( ));
				}
				entry.topicsetby = token;
				/* get rest of the declaration into the topic string, this is a topic */
				entry.topic = sep2.GetRemaining ( );
			} else if (token == "modes")
			{
				/* modes token, used for representing modes, it's the easier one, just load remaining data */
				if (sep2.StreamEnd ( ))
				{
					ServerInstance->Logs->Log ("MODULE", DEFAULT, "malformed modes declaration in channel database for channel %s", entry.name.c_str ( ));
					continue;
				}
				entry.modes = sep2.GetRemaining ( );
			}
		}
		/* return entry address */
		return &entry;
	}
};
/* class being a database writer, gets a database file name on construct */
class DatabaseWriter
{
	/* file stream */
	FILE *fd;
	std::string dbname, tmpname;
	/* public */
	public:
	/* constructor */
	DatabaseWriter (std::string filename)
	{
		fd = NULL;
		dbname = filename;
		tmpname = filename + ".tmp";
		/* ready, open temporary database */
		fd = fopen (tmpname.c_str ( ), "w");
		if (!fd)
		{
			ServerInstance->Logs->Log ("MODULE", DEFAULT, "cannot save to the channel database");
		}
	}
	/* destructor */
	~DatabaseWriter ( )
	{
		if (fd)
		{
			/* saving has been ended, close the database and flush buffers */
			fclose (fd);
			/* rename the database file */
			if (rename (tmpname.c_str ( ), dbname.c_str ( )) == -1)
			{
				ServerInstance->Logs->Log ("MODULE", DEFAULT, "can't rename the database file");
			}
		}
	}
	/* save the single channel */
	void next (Channel *chan)
	{
		if (!fd) return;
		/* first, construct the chaninfo line */
		std::string line;
		line.append ("chaninfo ").append (chan->name).append (" ").append (ConvToStr(chan->age)).append ("\n");
		if (fputs (line.c_str ( ), fd) == EOF)
		{
			ServerInstance->Logs->Log ("MODULE", DEFAULT, "unable to write the channel entry");
			fclose (fd);
			fd = NULL;
			return;
		}
		/* now, write the topic if present */
		if (!chan->topic.empty ( ))
		{
			line.clear ( );
			line.append ("topic ").append (ConvToStr (chan->topicset)).append (" ").append (chan->setby).append (" ").append (chan->topic).append ("\n");
			if (fputs (line.c_str ( ), fd) == EOF)
			{
				ServerInstance->Logs->Log ("MODULE", DEFAULT, "unable to write channel's topic declaration");
				fclose (fd);
				fd = NULL;
				return;
			}
		}
		/* get all channel modes */
		line.clear ( );
		irc::modestacker ms;
		chan->ChanModes (ms, MODELIST_FULL);
		line.append ("modes ").append (ms.popModeLine (FORMAT_PERSIST, INT_MAX, INT_MAX)).append ("\nend\n");
		if (fputs (line.c_str ( ), fd) == EOF)
		{
			ServerInstance->Logs->Log ("MODULE", DEFAULT, "can't write mode declaration");
			fclose (fd);
			fd = NULL;
			return;
		}
	}
};

class FlatFileChannelDB : public Module
{
 private:
	std::string filedb;
	bool dirty; // filedb needs to be flushed to disk
	bool storeregistered, storepermanent;

	/** Whether or not to store a channel to the database */
	bool ShouldStoreChannel(Channel* c)
	{
		return (storeregistered && c->IsModeSet("registered")) || (storepermanent && c->IsModeSet("permanent"));
	}

	void WriteFileDatabase ( )
	{
		// Dump entire database; open/close in constructor/destructor
		DatabaseWriter db (filedb);
		for (chan_hash::const_iterator i = ServerInstance->chanlist->begin(); i != ServerInstance->chanlist->end(); i++)
		{
			if (ShouldStoreChannel(i->second))
				db.next(i->second);
		}
	}

	/** Read flat-file database */
	void ReadFileDatabase ( )
	{
		/* create the reader object and open the database */
		DatabaseReader db (filedb);
		/* start the database read loop */
		EntryDescriptor *entry;
		while ((entry = db.next ( )))
		{
			/* we get a database entry */
			/* so if channel name doesn't start with #, give an error and continue */
			if (entry->name[0] != '#')
			{
				ServerInstance->Logs->Log ("MODULE", DEFAULT, "invalid channel name in channel database");
				continue;
			}
			/* entry is valid */
			/* try to find the channel */
			Channel *chan = ServerInstance->FindChan (entry->name);
			time_t ourTS = atol (entry->ts.c_str ( ));
			/* now, things that will be done depends on if channel was found or not */
			if (!chan)
			{
				/* channel does not exist, allocate it, it will be constructed and added to the network as an empty channel */
				chan = new Channel (entry->name, ourTS);
				/* empty modeless channel added and allocated */
			}
			else if(chan->age > ourTS)
			{
				chan = Channel::Nuke(chan, entry->name, ourTS);
			}
			else if(chan->age < ourTS)
			{
				continue; // there's a channel older than the one we want to load, so we don't load it
			}
			/* so, channel exists in the network, then if our topic is newer than the current, set data
			 * if no topic was ever set, topicset is 0 */
			time_t topicTS = atol (entry->topicset.c_str ( ));
			if (topicTS && topicTS >= chan->topicset)
			{
				chan->topic = entry->topic;
				chan->topicset = atol (entry->topicset.c_str ( ));
				chan->setby = entry->topicsetby;
				chan->WriteChannelWithServ(chan->setby, "TOPIC %s :%s", chan->name.c_str(), chan->topic.c_str());
			}
			/* if modestring is not empty, then set all modes in it */
			if (!entry->modes.empty ( ))
			{
				/* create sepstream */
				irc::spacesepstream sep(entry->modes);
				/* create modestacker for stacking all mode changes */
				irc::modestacker ms;
				/* spacesepstream is used to iterate through all modes that were saved for a channel */
				/* modestacker ms is used to stack them */
				/* iterate through the mode list */
				while (!sep.StreamEnd ( ))
				{
					/* get the mode to be applied */
					std::string token, name, value;
					sep.GetToken (token);
					/* the mode looks like name=value unless it doesn't have any value then it's just name */
					/* find the position of = sign */
					size_t namepos = token.find ('=');
					/* if we didn't find anything, set name to the token because mode is... valueless */
					if (namepos == std::string::npos) name = token;
					/* if it found that = sign, we have both name and value */
					else
					{
						/* it found it, name is string before the character, value is the string after it */
						name = token.substr (0, namepos);
						value = token.substr (namepos + 1);
					}
					/* mode name and value found, we can now add it to the modestacker */
					/* hmm, or we can't, reason is that letters are separate in user and channel modes, but mode names are not, so what if this thing we're setting is
					an user mode? */
					/* to check that, we must find the mode */
					ModeHandler *mc = ServerInstance->Modes->FindMode (name);
					/* those actions will be taken only if mode was found and if it's the channel mode */
					if (mc && mc->GetModeType ( ) == MODETYPE_CHANNEL)
					{
						/* mode was found and is not an user mode but a channel mode, push it to the modestacker, we use id instead of name for quicker push because we
						have the mode handler now */
						ms.push (irc::modechange (mc->id, value));
					}
				}
				/* okay, all modes were pushed, modestacker can now produce the mode line */
				/* so, process all mode changes and apply them on a channel as a merge */
				ServerInstance->Modes->Process (ServerInstance->FakeClient, chan, ms, true);
				ServerInstance->Modes->Send (ServerInstance->FakeClient, chan, ms);
				ServerInstance->PI->SendMode (ServerInstance->FakeClient, chan, ms, true);
			}
		}
	}

 public:
	/* module constructor, for initializing stuff */
	FlatFileChannelDB() : dirty(true)
	{
	}
	/* get module version and flags. */
	Version GetVersion()
	{
		return Version("Provides channel database storage in a flat-file format", VF_VENDOR);
	}
	void init ( )
	{
		Implementation eventlist[] = {
			I_OnBackgroundTimer, I_OnMode, I_OnPostTopicChange
		};

		ServerInstance->Modules->Attach(eventlist, this, sizeof(eventlist)/sizeof(Implementation));

		ReadFileDatabase();
	}
	/* rehash event */
	void ReadConfig(ConfigReadStatus& status)
	{
		ConfigTag *tag = ServerInstance->Config->GetTag ("chandb");
		filedb = tag->getString ("dbfile");
		storeregistered = tag->getBool("storeregistered", true);
		storepermanent = tag->getBool("storepermanent", false);
	}
	/* called on background timer to write all channels to disk if they were changed */
	void OnBackgroundTimer (time_t cur)
	{
		/* if not dirty then don't do anything */
		if (!dirty)
			return;
		/* dirty, one of registered channels was changed, save it */
		WriteFileDatabase();
		/* clear dirty to prevent next savings */
		dirty = false;
	}
	/* after the topic has been changed, call this to check if channel was registered */
	void OnPostTopicChange (User *user, Channel *chan, const std::string &topic)
	{
		if (ShouldStoreChannel(chan)) dirty = true;
	}
	/* this event is called after each modechange */
	void OnMode (User *user, Extensible *target, const irc::modestacker &modes)
	{
		Channel* chan = IS_CHANNEL(target);
		if (chan && ShouldStoreChannel(chan)) dirty = true;
	}

	void Prioritize()
	{
		// database reading may depend on channel modes being loaded
		ServerInstance->Modules->SetPriority(this, I_ModuleInit, PRIORITY_LAST);
	}
};
/* register the module */
MODULE_INIT(FlatFileChannelDB)
