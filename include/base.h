/*
Defines the base classes used by InspIRCd
*/

#ifndef __BASE_H__ 
#define __BASE_H__ 

#include "inspircd_config.h" 
#include <time.h>
#include <map>
#include <string>

typedef void* VoidPointer;
 
/** The base class for all inspircd classes
*/ 
class classbase
{
 public:
 	/** Time that the object was instantiated (used for TS calculation etc)
 	*/
	time_t age;

	/** Constructor,
	 * Sets the object's time
	 */
	classbase() { age = time(NULL); }
	~classbase() { }
};

/** class Extensible is the parent class of many classes such as userrec and chanrec.
 * class Extensible implements a system which allows modules to 'extend' the class by attaching data within
 * a map associated with the object. In this way modules can store their own custom information within user
 * objects, channel objects and server objects, without breaking other modules (this is more sensible than using
 * a flags variable, and each module defining bits within the flag as 'theirs' as it is less prone to conflict and
 * supports arbitary data storage).
 */
class Extensible : public classbase
{
	/** Private data store
	 */
	std::map<std::string,VoidPointer> Extension_Items;
};

#endif

