/*


*/

#include "inspircd_config.h" 
#include <time.h>
 
#ifndef __BASE_H__ 
#define __BASE_H__ 
 
class classbase
{
 public:
	time_t age;

	classbase() { age = time(NULL); }
	~classbase() { }
};

#endif

