/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2007 InspIRCd Development Team
 * See: http://www.inspircd.org/wiki/index.php/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#include "inspircd.h"
#include <mysql.h>
#include <pthread.h>
#include "users.h"
#include "channels.h"
#include "modules.h"
#include "m_sqlv2.h"

/* VERSION 2 API: With nonblocking (threaded) requests */

/* $ModDesc: SQL Service Provider module for all other m_sql* modules */
/* $CompileFlags: exec("mysql_config --include") */
/* $LinkerFlags: exec("mysql_config --libs_r") rpath("mysql_config --libs_r") */
/* $ModDep: m_sqlv2.h */

/* THE NONBLOCKING MYSQL API!
 * 
 * MySQL provides no nonblocking (asyncronous) API of its own, and its developers recommend
 * that instead, you should thread your program. This is what i've done here to allow for
 * asyncronous SQL requests via mysql. The way this works is as follows:
 *
 * The module spawns a thread via pthreads, and performs its mysql queries in this thread,
 * using a queue with priorities. There is a mutex on either end which prevents two threads
 * adjusting the queue at the same time, and crashing the ircd. Every 50 milliseconds, the
 * worker thread wakes up, and checks if there is a request at the head of its queue.
 * If there is, it processes this request, blocking the worker thread but leaving the ircd
 * thread to go about its business as usual. During this period, the ircd thread is able
 * to insert futher pending requests into the queue.
 *
 * Once the processing of a request is complete, it is removed from the incoming queue to
 * an outgoing queue, and initialized as a 'response'. The worker thread then signals the
 * ircd thread (via a loopback socket) of the fact a result is available, by sending the
 * connection ID through the connection.
 *
 * The ircd thread then mutexes the queue once more, reads the outbound response off the head
 * of the queue, and sends it on its way to the original calling module.
 *
 * XXX: You might be asking "why doesnt he just send the response from within the worker thread?"
 * The answer to this is simple. The majority of InspIRCd, and in fact most ircd's are not
 * threadsafe. This module is designed to be threadsafe and is careful with its use of threads,
 * however, if we were to call a module's OnRequest even from within a thread which was not the
 * one the module was originally instantiated upon, there is a chance of all hell breaking loose
 * if a module is ever put in a re-enterant state (stack corruption could occur, crashes, data
 * corruption, and worse, so DONT think about it until the day comes when InspIRCd is 100%
 * gauranteed threadsafe!)
 *
 * For a diagram of this system please see http://www.inspircd.org/wiki/Mysql2
 */


class SQLConnection;
class Notifier;


typedef std::map<std::string, SQLConnection*> ConnMap;
bool giveup = false;
static Module* SQLModule = NULL;
static Notifier* MessagePipe = NULL;
int QueueFD = -1;


#if !defined(MYSQL_VERSION_ID) || MYSQL_VERSION_ID<32224
#define mysql_field_count mysql_num_fields
#endif

typedef std::deque<SQLresult*> ResultQueue;

/* A mutex to wrap around queue accesses */
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t results_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t logging_mutex = PTHREAD_MUTEX_INITIALIZER;

/** Represents a mysql result set
 */
class MySQLresult : public SQLresult
{
	int currentrow;
	std::vector<std::string> colnames;
	std::vector<SQLfieldList> fieldlists;
	SQLfieldMap* fieldmap;
	SQLfieldMap fieldmap2;
	SQLfieldList emptyfieldlist;
	int rows;
 public:

	MySQLresult(Module* self, Module* to, MYSQL_RES* res, int affected_rows, unsigned int id) : SQLresult(self, to, id), currentrow(0), fieldmap(NULL)
	{
		/* A number of affected rows from from mysql_affected_rows.
		 */
		fieldlists.clear();
		rows = 0;
		if (affected_rows >= 1)
		{
			rows = affected_rows;
			fieldlists.resize(rows);
		}
		unsigned int field_count = 0;
		if (res)
		{
			MYSQL_ROW row;
			int n = 0;
			while ((row = mysql_fetch_row(res)))
			{
				if (fieldlists.size() < (unsigned int)rows+1)
				{
					fieldlists.resize(fieldlists.size()+1);
				}
				field_count = 0;
				MYSQL_FIELD *fields = mysql_fetch_fields(res);
				if(mysql_num_fields(res) == 0)
					break;
				if (fields && mysql_num_fields(res))
				{
					colnames.clear();
					while (field_count < mysql_num_fields(res))
					{
						std::string a = (fields[field_count].name ? fields[field_count].name : "");
						std::string b = (row[field_count] ? row[field_count] : "");
						SQLfield sqlf(b, !row[field_count]);
						colnames.push_back(a);
						fieldlists[n].push_back(sqlf); 
						field_count++;
					}
					n++;
				}
				rows++;
			}
			mysql_free_result(res);
		}
	}

	MySQLresult(Module* self, Module* to, SQLerror e, unsigned int id) : SQLresult(self, to, id), currentrow(0)
	{
		rows = 0;
		error = e;
	}

	~MySQLresult()
	{
	}

	virtual int Rows()
	{
		return rows;
	}

	virtual int Cols()
	{
		return colnames.size();
	}

	virtual std::string ColName(int column)
	{
		if (column < (int)colnames.size())
		{
			return colnames[column];
		}
		else
		{
			throw SQLbadColName();
		}
		return "";
	}

	virtual int ColNum(const std::string &column)
	{
		for (unsigned int i = 0; i < colnames.size(); i++)
		{
			if (column == colnames[i])
				return i;
		}
		throw SQLbadColName();
		return 0;
	}

	virtual SQLfield GetValue(int row, int column)
	{
		if ((row >= 0) && (row < rows) && (column >= 0) && (column < Cols()))
		{
			return fieldlists[row][column];
		}

		throw SQLbadColName();

		/* XXX: We never actually get here because of the throw */
		return SQLfield("",true);
	}

	virtual SQLfieldList& GetRow()
	{
		if (currentrow < rows)
			return fieldlists[currentrow];
		else
			return emptyfieldlist;
	}

	virtual SQLfieldMap& GetRowMap()
	{
		fieldmap2.clear();

		if (currentrow < rows)
		{
			for (int i = 0; i < Cols(); i++)
			{
				fieldmap2.insert(std::make_pair(colnames[i],GetValue(currentrow, i)));
			}
			currentrow++;
		}

		return fieldmap2;
	}

	virtual SQLfieldList* GetRowPtr()
	{
		SQLfieldList* fieldlist = new SQLfieldList();

		if (currentrow < rows)
		{
			for (int i = 0; i < Rows(); i++)
			{
				fieldlist->push_back(fieldlists[currentrow][i]);
			}
			currentrow++;
		}
		return fieldlist;
	}

	virtual SQLfieldMap* GetRowMapPtr()
	{
		fieldmap = new SQLfieldMap();
		
		if (currentrow < rows)
		{
			for (int i = 0; i < Cols(); i++)
			{
				fieldmap->insert(std::make_pair(colnames[i],GetValue(currentrow, i)));
			}
			currentrow++;
		}

		return fieldmap;
	}

	virtual void Free(SQLfieldMap* fm)
	{
		delete fm;
	}

	virtual void Free(SQLfieldList* fl)
	{
		delete fl;
	}
};

class SQLConnection;

void NotifyMainThread(SQLConnection* connection_with_new_result);

/** Represents a connection to a mysql database
 */
class SQLConnection : public classbase
{
 protected:

	MYSQL connection;
	MYSQL_RES *res;
	MYSQL_ROW row;
	SQLhost host;
	std::map<std::string,std::string> thisrow;
	bool Enabled;

 public:

	QueryQueue queue;
	ResultQueue rq;

	// This constructor creates an SQLConnection object with the given credentials, but does not connect yet.
	SQLConnection(const SQLhost &hi) : host(hi), Enabled(false)
	{
	}

	~SQLConnection()
	{
		Close();
	}

	// This method connects to the database using the credentials supplied to the constructor, and returns
	// true upon success.
	bool Connect()
	{
		unsigned int timeout = 1;
		mysql_init(&connection);
		mysql_options(&connection,MYSQL_OPT_CONNECT_TIMEOUT,(char*)&timeout);
		return mysql_real_connect(&connection, host.host.c_str(), host.user.c_str(), host.pass.c_str(), host.name.c_str(), host.port, NULL, 0);
	}

	void DoLeadingQuery()
	{
		if (!CheckConnection())
			return;

		/* Parse the command string and dispatch it to mysql */
		SQLrequest& req = queue.front();

		/* Pointer to the buffer we screw around with substitution in */
		char* query;

		/* Pointer to the current end of query, where we append new stuff */
		char* queryend;

		/* Total length of the unescaped parameters */
		unsigned long paramlen;

		/* Total length of query, used for binary-safety in mysql_real_query */
		unsigned long querylength = 0;

		paramlen = 0;

		for(ParamL::iterator i = req.query.p.begin(); i != req.query.p.end(); i++)
		{
			paramlen += i->size();
		}

		/* To avoid a lot of allocations, allocate enough memory for the biggest the escaped query could possibly be.
		 * sizeofquery + (totalparamlength*2) + 1
		 *
		 * The +1 is for null-terminating the string for mysql_real_escape_string
		 */

		query = new char[req.query.q.length() + (paramlen*2) + 1];
		queryend = query;

		/* Okay, now we have a buffer large enough we need to start copying the query into it and escaping and substituting
		 * the parameters into it...
		 */

		for(unsigned long i = 0; i < req.query.q.length(); i++)
		{
			if(req.query.q[i] == '?')
			{
				/* We found a place to substitute..what fun.
				 * use mysql calls to escape and write the
				 * escaped string onto the end of our query buffer,
				 * then we "just" need to make sure queryend is
				 * pointing at the right place.
				 */
				if(req.query.p.size())
				{
					unsigned long len = mysql_real_escape_string(&connection, queryend, req.query.p.front().c_str(), req.query.p.front().length());

					queryend += len;
					req.query.p.pop_front();
				}
				else
					break;
			}
			else
			{
				*queryend = req.query.q[i];
				queryend++;
			}
			querylength++;
		}

		*queryend = 0;

		pthread_mutex_lock(&queue_mutex);
		req.query.q = query;
		pthread_mutex_unlock(&queue_mutex);

		if (!mysql_real_query(&connection, req.query.q.data(), req.query.q.length()))
		{
			/* Successfull query */
			res = mysql_use_result(&connection);
			unsigned long rows = mysql_affected_rows(&connection);
			MySQLresult* r = new MySQLresult(SQLModule, req.GetSource(), res, rows, req.id);
			r->dbid = this->GetID();
			r->query = req.query.q;
			/* Put this new result onto the results queue.
			 * XXX: Remember to mutex the queue!
			 */
			pthread_mutex_lock(&results_mutex);
			rq.push_back(r);
			pthread_mutex_unlock(&results_mutex);
		}
		else
		{
			/* XXX: See /usr/include/mysql/mysqld_error.h for a list of
			 * possible error numbers and error messages */
			SQLerror e(QREPLY_FAIL, ConvToStr(mysql_errno(&connection)) + std::string(": ") + mysql_error(&connection));
			MySQLresult* r = new MySQLresult(SQLModule, req.GetSource(), e, req.id);
			r->dbid = this->GetID();
			r->query = req.query.q;

			pthread_mutex_lock(&results_mutex);
			rq.push_back(r);
			pthread_mutex_unlock(&results_mutex);
		}

		/* Now signal the main thread that we've got a result to process.
		 * Pass them this connection id as what to examine
		 */

		delete[] query;

		NotifyMainThread(this);
	}

	bool ConnectionLost()
	{
		if (&connection) {
			return (mysql_ping(&connection) != 0);
		}
		else return false;
	}

	bool CheckConnection()
	{
		if (ConnectionLost()) {
			return Connect();
		}
		else return true;
	}

	std::string GetError()
	{
		return mysql_error(&connection);
	}

	const std::string& GetID()
	{
		return host.id;
	}

	std::string GetHost()
	{
		return host.host;
	}

	void SetEnable(bool Enable)
	{
		Enabled = Enable;
	}

	bool IsEnabled()
	{
		return Enabled;
	}

	void Close()
	{
		mysql_close(&connection);
	}

	const SQLhost& GetConfHost()
	{
		return host;
	}

};

ConnMap Connections;

bool HasHost(const SQLhost &host)
{
	for (ConnMap::iterator iter = Connections.begin(); iter != Connections.end(); iter++)
	{
		if (host == iter->second->GetConfHost())
			return true;
	}
	return false;
}

bool HostInConf(ConfigReader* conf, const SQLhost &h)
{
	for(int i = 0; i < conf->Enumerate("database"); i++)
	{
		SQLhost host;
		host.id		= conf->ReadValue("database", "id", i);
		host.host	= conf->ReadValue("database", "hostname", i);
		host.port	= conf->ReadInteger("database", "port", i, true);
		host.name	= conf->ReadValue("database", "name", i);
		host.user	= conf->ReadValue("database", "username", i);
		host.pass	= conf->ReadValue("database", "password", i);
		host.ssl	= conf->ReadFlag("database", "ssl", i);
		if (h == host)
			return true;
	}
	return false;
}

void ClearOldConnections(ConfigReader* conf)
{
	ConnMap::iterator i,safei;
	for (i = Connections.begin(); i != Connections.end(); i++)
	{
		if (!HostInConf(conf, i->second->GetConfHost()))
		{
			DELETE(i->second);
			safei = i;
			--i;
			Connections.erase(safei);
		}
	}
}

void ClearAllConnections()
{
	ConnMap::iterator i;
	while ((i = Connections.begin()) != Connections.end())
	{
		Connections.erase(i);
		DELETE(i->second);
	}
}

void ConnectDatabases(InspIRCd* ServerInstance)
{
	for (ConnMap::iterator i = Connections.begin(); i != Connections.end(); i++)
	{
		if (i->second->IsEnabled())
			continue;

		i->second->SetEnable(true);
		if (!i->second->Connect())
		{
			/* XXX: MUTEX */
			pthread_mutex_lock(&logging_mutex);
			ServerInstance->Log(DEFAULT,"SQL: Failed to connect database "+i->second->GetHost()+": Error: "+i->second->GetError());
			i->second->SetEnable(false);
			pthread_mutex_unlock(&logging_mutex);
		}
	}
}

void LoadDatabases(ConfigReader* conf, InspIRCd* ServerInstance)
{
	ClearOldConnections(conf);
	for (int j =0; j < conf->Enumerate("database"); j++)
	{
		SQLhost host;
		host.id		= conf->ReadValue("database", "id", j);
		host.host	= conf->ReadValue("database", "hostname", j);
		host.port	= conf->ReadInteger("database", "port", j, true);
		host.name	= conf->ReadValue("database", "name", j);
		host.user	= conf->ReadValue("database", "username", j);
		host.pass	= conf->ReadValue("database", "password", j);
		host.ssl	= conf->ReadFlag("database", "ssl", j);

		if (HasHost(host))
			continue;

		if (!host.id.empty() && !host.host.empty() && !host.name.empty() && !host.user.empty() && !host.pass.empty())
		{
			SQLConnection* ThisSQL = new SQLConnection(host);
			Connections[host.id] = ThisSQL;
		}
	}
	ConnectDatabases(ServerInstance);
}

char FindCharId(const std::string &id)
{
	char i = 1;
	for (ConnMap::iterator iter = Connections.begin(); iter != Connections.end(); ++iter, ++i)
	{
		if (iter->first == id)
		{
			return i;
		}
	}
	return 0;
}

ConnMap::iterator GetCharId(char id)
{
	char i = 1;
	for (ConnMap::iterator iter = Connections.begin(); iter != Connections.end(); ++iter, ++i)
	{
		if (i == id)
			return iter;
	}
	return Connections.end();
}

void NotifyMainThread(SQLConnection* connection_with_new_result)
{
	/* Here we write() to the socket the main thread has open
	 * and we connect()ed back to before our thread became active.
	 * The main thread is using a nonblocking socket tied into
	 * the socket engine, so they wont block and they'll receive
	 * nearly instant notification. Because we're in a seperate
	 * thread, we can just use standard connect(), and we can
	 * block if we like. We just send the connection id of the
	 * connection back.
	 *
	 * NOTE: We only send a single char down the connection, this
	 * way we know it wont get a partial read at the other end if
	 * the system is especially congested (see bug #263).
	 * The function FindCharId translates a connection name into a
	 * one character id, and GetCharId translates a character id
	 * back into an iterator.
	 */
	char id = FindCharId(connection_with_new_result->GetID());
	send(QueueFD, &id, 1, 0);
}

void* DispatcherThread(void* arg);

/** Used by m_mysql to notify one thread when the other has a result
 */
class Notifier : public InspSocket
{
	insp_sockaddr sock_us;
	socklen_t uslen;
	

 public:

	/* Create a socket on a random port. Let the tcp stack allocate us an available port */
#ifdef IPV6
	Notifier(InspIRCd* SI) : InspSocket(SI, "::1", 0, true, 3000)
#else
	Notifier(InspIRCd* SI) : InspSocket(SI, "127.0.0.1", 0, true, 3000)
#endif
	{
		uslen = sizeof(sock_us);
		if (getsockname(this->fd,(sockaddr*)&sock_us,&uslen))
		{
			throw ModuleException("Could not create random listening port on localhost");
		}
	}

	Notifier(InspIRCd* SI, int newfd, char* ip) : InspSocket(SI, newfd, ip)
	{
	}

	/* Using getsockname and ntohs, we can determine which port number we were allocated */
	int GetPort()
	{
#ifdef IPV6
		return ntohs(sock_us.sin6_port);
#else
		return ntohs(sock_us.sin_port);
#endif
	}

	virtual int OnIncomingConnection(int newsock, char* ip)
	{
		Notifier* n = new Notifier(this->Instance, newsock, ip);
		n = n; /* Stop bitching at me, GCC */
		return true;
	}

	virtual bool OnDataReady()
	{
		char data = 0;
		/* NOTE: Only a single character is read so we know we
		 * cant get a partial read. (We've been told that theres
		 * data waiting, so we wont ever get EAGAIN)
		 * The function GetCharId translates a single character
		 * back into an iterator.
		 */
		if (read(this->GetFd(), &data, 1) > 0)
		{
			ConnMap::iterator iter = GetCharId(data);
			if (iter != Connections.end())
			{
				/* Lock the mutex, send back the data */
				pthread_mutex_lock(&results_mutex);
				ResultQueue::iterator n = iter->second->rq.begin();
				(*n)->Send();
				iter->second->rq.pop_front();
				pthread_mutex_unlock(&results_mutex);
				return true;
			}
			/* No error, but unknown id */
			return true;
		}

		/* Erk, error on descriptor! */
		return false;
	}
};

/** MySQL module
 */
class ModuleSQL : public Module
{
 public:
	
	ConfigReader *Conf;
	InspIRCd* PublicServerInstance;
	pthread_t Dispatcher;
	int currid;
	bool rehashing;

	ModuleSQL(InspIRCd* Me)
	: Module::Module(Me), rehashing(false)
	{
		ServerInstance->UseInterface("SQLutils");

		Conf = new ConfigReader(ServerInstance);
		PublicServerInstance = ServerInstance;
		currid = 0;
		SQLModule = this;

		MessagePipe = new Notifier(ServerInstance);
		
		pthread_attr_t attribs;
		pthread_attr_init(&attribs);
		pthread_attr_setdetachstate(&attribs, PTHREAD_CREATE_DETACHED);
		if (pthread_create(&this->Dispatcher, &attribs, DispatcherThread, (void *)this) != 0)
		{
			throw ModuleException("m_mysql: Failed to create dispatcher thread: " + std::string(strerror(errno)));
		}

		if (!ServerInstance->PublishFeature("SQL", this))
		{
			/* Tell worker thread to exit NOW */
			giveup = true;
			throw ModuleException("m_mysql: Unable to publish feature 'SQL'");
		}

		ServerInstance->PublishInterface("SQL", this);
	}

	virtual ~ModuleSQL()
	{
		giveup = true;
		ClearAllConnections();
		DELETE(Conf);
		ServerInstance->UnpublishInterface("SQL", this);
		ServerInstance->UnpublishFeature("SQL");
		ServerInstance->DoneWithInterface("SQLutils");
	}


	void Implements(char* List)
	{
		List[I_OnRehash] = List[I_OnRequest] = 1;
	}

	unsigned long NewID()
	{
		if (currid+1 == 0)
			currid++;
		return ++currid;
	}

	char* OnRequest(Request* request)
	{
		if(strcmp(SQLREQID, request->GetId()) == 0)
		{
			SQLrequest* req = (SQLrequest*)request;

			/* XXX: Lock */
			pthread_mutex_lock(&queue_mutex);

			ConnMap::iterator iter;

			char* returnval = NULL;

			if((iter = Connections.find(req->dbid)) != Connections.end())
			{
				req->id = NewID();
				iter->second->queue.push(*req);
				returnval = SQLSUCCESS;
			}
			else
			{
				req->error.Id(BAD_DBID);
			}

			pthread_mutex_unlock(&queue_mutex);
			/* XXX: Unlock */

			return returnval;
		}

		return NULL;
	}

	virtual void OnRehash(userrec* user, const std::string &parameter)
	{
		rehashing = true;
	}
	
	virtual Version GetVersion()
	{
		return Version(1,1,0,0,VF_VENDOR|VF_SERVICEPROVIDER,API_VERSION);
	}
	
};

void* DispatcherThread(void* arg)
{
	ModuleSQL* thismodule = (ModuleSQL*)arg;
	LoadDatabases(thismodule->Conf, thismodule->PublicServerInstance);

	/* Connect back to the Notifier */

	if ((QueueFD = socket(AF_FAMILY, SOCK_STREAM, 0)) == -1)
	{
		/* crap, we're out of sockets... */
		return NULL;
	}

	insp_sockaddr addr;

#ifdef IPV6
	insp_aton("::1", &addr.sin6_addr);
	addr.sin6_family = AF_FAMILY;
	addr.sin6_port = htons(MessagePipe->GetPort());
#else
	insp_inaddr ia;
	insp_aton("127.0.0.1", &ia);
	addr.sin_family = AF_FAMILY;
	addr.sin_addr = ia;
	addr.sin_port = htons(MessagePipe->GetPort());
#endif

	if (connect(QueueFD, (sockaddr*)&addr,sizeof(addr)) == -1)
	{
		/* wtf, we cant connect to it, but we just created it! */
		return NULL;
	}

	while (!giveup)
	{
		if (thismodule->rehashing)
		{
		/* XXX: Lock */
			pthread_mutex_lock(&queue_mutex);
			thismodule->rehashing = false;
			LoadDatabases(thismodule->Conf, thismodule->PublicServerInstance);
			pthread_mutex_unlock(&queue_mutex);
			/* XXX: Unlock */
		}

		SQLConnection* conn = NULL;
		/* XXX: Lock here for safety */
		pthread_mutex_lock(&queue_mutex);
		for (ConnMap::iterator i = Connections.begin(); i != Connections.end(); i++)
		{
			if (i->second->queue.totalsize())
			{
				conn = i->second;
				break;
			}
		}
		pthread_mutex_unlock(&queue_mutex);
		/* XXX: Unlock */

		/* Theres an item! */
		if (conn)
		{
			conn->DoLeadingQuery();

			/* XXX: Lock */
			pthread_mutex_lock(&queue_mutex);
			conn->queue.pop();
			pthread_mutex_unlock(&queue_mutex);
			/* XXX: Unlock */
		}

		usleep(50);
	}

	return NULL;
}

MODULE_INIT(ModuleSQL);

