/*


*/

#include "inspircd_config.h" 
#include <time.h>
 
#ifndef __BASE_H__ 
#define __BASE_H__ 


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

#endif

