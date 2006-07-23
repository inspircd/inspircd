#ifndef INSPIRCD_SQLUTILS
#define INSPIRCD_SQLUTILS

#include "modules.h"

#define SQLUTILAU "SQLutil AssociateUser"
#define SQLUTILAC "SQLutil AssociateChan"
#define SQLUTILUA "SQLutil UnAssociate"
#define SQLUTILGU "SQLutil GetAssocUser"
#define SQLUTILGC "SQLutil GetAssocChan"
#define SQLUTILSUCCESS "You shouldn't be reading this (success)"

class AssociateUser : public Request
{
public:
	unsigned long id;
	userrec* user;
	
	AssociateUser(Module* s, Module* d, unsigned long i, userrec* u)
	: Request(s, d, SQLUTILAU), id(i), user(u)
	{
	}
	
	AssociateUser& S()
	{
		Send();
		return *this;
	}
};

class AssociateChan : public Request
{
public:
	unsigned long id;
	chanrec* chan;
	
	AssociateChan(Module* s, Module* d, unsigned long i, chanrec* u)
	: Request(s, d, SQLUTILAC), id(i), chan(u)
	{
	}
	
	AssociateChan& S()
	{
		Send();
		return *this;
	}
};

class UnAssociate : public Request
{
public:
	unsigned long id;

	UnAssociate(Module* s, Module* d, unsigned long i)
	: Request(s, d, SQLUTILUA), id(i)
	{
	}
	
	UnAssociate& S()
	{
		Send();
		return *this;
	}
};

class GetAssocUser : public Request
{
public:
	unsigned long id;
	userrec* user;

	GetAssocUser(Module* s, Module* d, unsigned long i)
	: Request(s, d, SQLUTILGU), id(i), user(NULL)
	{
	}
	
	GetAssocUser& S()
	{
		Send();
		return *this;
	}
};

class GetAssocChan : public Request
{
public:
	unsigned long id;
	chanrec* chan;

	GetAssocChan(Module* s, Module* d, unsigned long i)
	: Request(s, d, SQLUTILGC), id(i), chan(NULL)
	{
	}
	
	GetAssocChan& S()
	{
		Send();
		return *this;
	}
};

#endif
