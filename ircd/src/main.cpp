//
//  main.cpp
//  ircd
//
//  Created by Paige Thompson on 3/17/23.
//

#include "inspircd.h"
#include <signal.h>

#include <unistd.h>
#include <sys/resource.h>
#include <getopt.h>
#include <pwd.h> // setuid
#include <grp.h> // setgid
#include <fstream>
#include <iostream>
#include "xline.h"
#include "exitcodes.h"

/* On posix systems, the flow of the program starts right here, with
 * ENTRYPOINT being a #define that defines main(). On Windows, ENTRYPOINT
 * defines smain() and the real main() is in the service code under
 * win32service.cpp. This allows the service control manager to control
 * the process where we are running as a windows service.
 */

ENTRYPOINT {
    InspIRCd::__run(argc, argv);
    return 0;
}
