/*
 *
 * (C) 2003-2023 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 *
 * Based on the original code of Epona by Lara.
 * Based on the original code of Services by Andy Church.
 */

#ifndef SOCKETENGINE_H
#define SOCKETENGINE_H

#include "services.h"
#include "sockets.h"

class CoreExport SocketEngine {
    static const int DefaultSize = 2; // Uplink, mode stacker
  public:
    /* Map of sockets */
    static std::map<int, Socket *> Sockets;

    /** Called to initialize the socket engine
     */
    static void Init();

    /** Called to shutdown the socket engine
     */
    static void Shutdown();

    /** Set a flag on a socket
     * @param s The socket
     * @param set Whether setting or unsetting
     * @param flag The flag to set or unset
     */
    static void Change(Socket *s, bool set, SocketFlag flag);

    /** Read from sockets and do things
     */
    static void Process();

    static int GetLastError();
    static void SetLastError(int);

    static bool IgnoreErrno();
};

#endif // SOCKETENGINE_H
