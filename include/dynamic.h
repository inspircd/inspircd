/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  Inspire is copyright (C) 2002-2004 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *           	  <Craig@chatspike.net>
 *     
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */


#ifndef __DLL_H
#define __DLL_H


class DLLManager
{
 public:
	DLLManager( const char *fname );
	virtual ~DLLManager();


	bool GetSymbol( void **, const char *sym_name );

	const char *LastError() 
	{
		 return err;
	}
	
 protected:
	void *h;
	const char *err;
};


class DLLFactoryBase : public DLLManager
{
 public:
	DLLFactoryBase(const char *fname, const char *func_name = 0);
	virtual ~DLLFactoryBase();
	void * (*factory_func)(void);	
};


template <class T> class DLLFactory : public DLLFactoryBase
{
 public:
	DLLFactory(const char *fname, const char *func_name=0) : DLLFactoryBase(fname,func_name)
	{
		if (factory_func)
			factory = (T*)factory_func();
		else
			factory = 0;
	}
	
	~DLLFactory()
	{
		delete factory;
	}

	T *factory;
};






#endif
