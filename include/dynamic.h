#ifndef __DLL_H
#define __DLL_H

//
// class DLLManager is the simple ELF C++ Library manager.
//
// It tries to dynamically load the specified shared library
// when it is construted.
//
// You should call LastError() before doing anything.  If it 
// returns NULL there is no error.
//


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


//
// class DLLFactoryBase is the base class used for the DLLFactory
// template class.  
// 
// It inherits from the DLLManager class and must be constructed with
// the file name of the shared library and the function name within that
// library which will create the desired C++ factory class.
// If you do not provide func_name to the constructor, it defaults to
// the undecorated "C" symbol "factory0"
//
// factory_func will be set to a pointer to the requested factory creator 
// function.  If there was an error linking to the shared library,
// factory_func will be 0.
//
// You can call 'LastError()' to find the error message that occurred.
//
//

class DLLFactoryBase : public DLLManager
{
 public:
	DLLFactoryBase(const char *fname, const char *func_name = 0);
	virtual ~DLLFactoryBase();
	void * (*factory_func)(void);	
};


//
// The DLLFactory template class inherits from DLLFactoryBase.
// The constructor takes the file name of the shared library
// and the undecorated "C" symbol name of the factory creator
// function.  The factory creator function in your shared library
// MUST either return a pointer to an object that is a subclass
// of 'T' or it must return 0.
//
// If everything is cool, then 'factory' will point to the
// requested factory class.  If not, it will be 0.
//
// Since the DLLFactory template ultimately inherits DLLManager,
// you can call LastError() to get any error code information
//
// The created factory is OWNED by the DLLFactory class.  
// The created factory will get deleted when the DLLFactory class
// is deleted, because the DLL will get unloaded as well.
//

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
