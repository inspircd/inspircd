/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  Inspire is copyright (C) 2002-2005 ChatSpike-Dev.
 *                       E-mail:
 *                <brain@chatspike.net>
 *                <Craig@chatspike.net>
 *
 * Written by Craig Edwards, Craig McLure, and others.
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

// Fill the engine with client file descriptors pending an action

#define epoll_fill      int i = epoll_wait(ep, event, 1, 5); \
                        if (i > 0) \
                        { \
                                log(DEBUG,"epoll_wait call: ep=%d, i=%d",ep,i); \
                                userrec* cu = fd_ref_table[event[0].data.fd];

#define kqueue_fill     ts.tv_sec = 0; \
                        ts.tv_nsec = 1000L; \
                        int i = kevent(kq, NULL, 0, &ke, 1, &ts); \
                        if (i > 0) \
                        { \
                                log(DEBUG,"kevent call: kq=%d, i=%d",kq,i); \
                                userrec* cu = fd_ref_table[ke.ident];

#define select_fill     tval.tv_sec = 0; \
                        tval.tv_usec = 1000L; \
                        selectResult2 = select(FD_SETSIZE, &sfd, NULL, NULL, &tval); \
                        if ((selectResult2 > 0) && (xcount != clientlist.end())) \
                        for (user_hash::iterator count2a = xcount; count2a != endingiter; count2a++) \
                        { \
                                if (count2a == clientlist.end()) \
                                        break; \
                                userrec* cu = count2a->second;

#ifdef USE_EPOLL
#define engine_fill epoll_fill
#endif
#ifdef USE_KQUEUE
#define engine_fill kqueue_fill
#endif
#ifdef USE_SELECT
#define engine_fill select_fill
#endif

// how to determine if a socket needs attention if further checks are needed

#define epoll_check    ((cu->fd != FD_MAGIC_NUMBER) && (cu->fd != -1))

#define kqueue_check   ((cu->fd != FD_MAGIC_NUMBER) && (cu->fd != -1))

#define select_check   ((cu->fd != FD_MAGIC_NUMBER) && (cu->fd != -1) && (FD_ISSET (cu->fd, &sfd)))

#ifdef USE_EPOLL
#define engine_check epoll_check
#endif
#ifdef USE_KQUEUE
#define engine_check kqueue_check
#endif
#ifdef USE_SELECT
#define engine_check select_check
#endif

// how to clean up an exiting client

#define epoll_cleanup                   log(DEBUG,"InspIRCd: Exited: %s",cu->nick); \
                                        kill_link(cu,"Client exited"); \
                                        log(DEBUG,"Bailing from client exit"); \
                                        goto label;

#define kqueue_cleanup                  log(DEBUG,"InspIRCd: Exited: %s",cu->nick); \
                                        kill_link(cu,"Client exited"); \
                                        log(DEBUG,"Bailing from client exit"); \
                                        goto label;

#define select_cleanup                  if (count2->second) \
                                        { \
                                                log(DEBUG,"InspIRCd: Exited: %s",cu->nick); \
                                                kill_link(cu,"Client exited"); \
                                                log(DEBUG,"Bailing from client exit"); \
                                                goto label; \
                                        }

#ifdef USE_EPOLL
#define engine_cleanup epoll_cleanup
#endif
#ifdef USE_KQUEUE
#define engine_cleanup kqueue_cleanup
#endif
#ifdef USE_SELECT
#define engine_cleanup select_cleanup
#endif


// how to scan the set for fd's requiring action

#define select_scanset          for (count = 0; count < boundPortCount; count++) \
			        { \
			                FD_SET (openSockfd[count], &selectFds); \
			        } \
			        tv.tv_usec = 30000L; \
			        selectResult = select(MAXSOCKS, &selectFds, NULL, NULL, &tv); \
			        if (selectResult > 0) \
			        { \
			                for (count = 0; count < boundPortCount; count++) \
		                { \
			                        if (FD_ISSET (openSockfd[count], &selectFds)) \
			                        {

#define kqueue_scanset          ts.tv_sec = 0; \
			        ts.tv_nsec = 30000L; \
			        i = kevent(lkq, NULL, 0, ke_list, 32, &ts); \
			        if (i > 0) for (j = 0; j < i; j++) \
			        { \
			                log(DEBUG,"kqueue: Listening socket event, i=%d, ke.ident=%d",i,ke_list[j].ident); \
			                for (count = 0; count < boundPortCount; count++) \
			                { \
			                        if ((unsigned)ke_list[j].ident == (unsigned)openSockfd[count]) \
			                        {

#define epoll_scanset           i = epoll_wait(lep, event, 32, EP_DELAY); \
			        if (i > 0) for (j = 0; j < i; j++) \
			        { \
			                log(DEBUG,"epoll: Listening socket event, i=%d,events[j].data.fd=%d",i,event[j].data.fd); \
			                for (count = 0; count < boundPortCount; count++) \
			                { \
			                        if ((unsigned)event[j].data.fd == (unsigned)openSockfd[count]) \
			                        {

#ifdef USE_EPOLL
#define engine_scanset epoll_scanset
#endif
#ifdef USE_KQUEUE
#define engine_scanset kqueue_scanset
#endif
#ifdef USE_SELECT
#define engine_scanset select_scanset
#endif

// a list of variables used specifically by this engine

#define kqueue_structs struct kevent ke; \
        struct kevent ke_list[33]; \
        struct timespec ts;

#define epoll_structs struct epoll_event event[33];

#define select_structs fd_set sfd;

#ifdef USE_EPOLL
#define engine_structs epoll_structs
#endif
#ifdef USE_KQUEUE
#define engine_structs kqueue_structs
#endif
#ifdef USE_SELECT
#define engine_structs select_structs
#endif

// how to initialise the engine before using it

#define select_init     while(0);

#define kqueue_init     kq = kqueue(); \
		        lkq = kqueue(); \
		        skq = kqueue(); \
		        if ((kq == -1) || (lkq == -1) || (skq == -1)) \
		        { \
		                log(DEFAULT,"main: kqueue() failed!"); \
		                printf("ERROR: could not initialise kqueue event system. Shutting down.\n"); \
		                Exit(ERROR); \
		        }

#define epoll_init      ep = epoll_create(MAXCLIENTS); \
		        lep = epoll_create(32); \
		        sep = epoll_create(128); \
		        if ((ep == -1) || (lep == -1) || (sep == -1)) \
		        { \
		                log(DEFAULT,"main: epoll_create() failed!"); \
		                printf("ERROR: could not initialise epoll event system. Shutting down.\n"); \
		                Exit(ERROR); \
		        }


#ifdef USE_EPOLL
#define engine_init epoll_init
#endif
#ifdef USE_KQUEUE
#define engine_init kqueue_init
#endif
#ifdef USE_SELECT
#define engine_init select_init
#endif

// how to delete a client fd from the engine

#define kqueue_delete_fd        struct kevent ke; \
		                EV_SET(&ke, user->fd, EVFILT_READ, EV_DELETE, 0, 0, NULL); \
		                int i = kevent(kq, &ke, 1, 0, 0, NULL); \
		                if (i == -1) \
		                { \
		                        log(DEBUG,"kqueue: Failed to remove user from queue!"); \
		                }

#define epoll_delete_fd         struct epoll_event ev; \
		                ev.events = EPOLLIN | EPOLLET; \
		                ev.data.fd = user->fd; \
		                int i = epoll_ctl(ep, EPOLL_CTL_DEL, user->fd, &ev); \
		                if (i < 0) \
		                { \
		                        log(DEBUG,"epoll: List deletion failure!"); \
		                }

#define select_delete_fd        while(0);


#ifdef USE_EPOLL
#define engine_delete_fd epoll_delete_fd
#endif
#ifdef USE_KQUEUE
#define engine_delete_fd kqueue_delete_fd
#endif
#ifdef USE_SELECT
#define engine_delete_fd select_delete_fd
#endif

// how to add a client fd to the engine

#define select_add_fd		while(0);

#define epoll_add_fd            struct epoll_event ev; \
			        log(DEBUG,"epoll: Adduser to events, ep=%d socket=%d",ep,socket); \
			        ev.events = EPOLLIN | EPOLLET; \
			        ev.data.fd = socket; \
			        int i = epoll_ctl(ep, EPOLL_CTL_ADD, socket, &ev); \
			        if (i < 0) \
			        { \
			                log(DEBUG,"epoll: List insertion failure!"); \
			        }

#define kqueue_add_fd           struct kevent ke; \
			        log(DEBUG,"kqueue: Add user to events, kq=%d socket=%d",kq,socket);\
			        EV_SET(&ke, socket, EVFILT_READ, EV_ADD, 0, 0, NULL); \
			        int i = kevent(kq, &ke, 1, 0, 0, NULL); \
			        if (i == -1) \
			        { \
			                log(DEBUG,"kqueue: List insertion failure!"); \
			        }

#ifdef USE_EPOLL
#define engine_add_fd epoll_add_fd
#endif
#ifdef USE_KQUEUE
#define engine_add_fd kqueue_add_fd
#endif
#ifdef USE_SELECT
#define engine_add_fd select_add_fd
#endif

#define	select_server_fill	log(DEFAULT,"Using standard select socket engine.");

#define epoll_server_fill        log(DEFAULT,"epoll socket engine is enabled. Filling listen list. boundPortcount=%d",boundPortCount); \
			        for (count = 0; count < boundPortCount; count++) \
			        { \
			                struct epoll_event ev; \
			                log(DEBUG,"epoll: Add listening socket to events, ep=%d socket=%d",lep,openSockfd[count]); \
			                ev.events = EPOLLIN | EPOLLET; \
			                ev.data.fd = openSockfd[count]; \
			                int i = epoll_ctl(lep, EPOLL_CTL_ADD, openSockfd[count], &ev); \
			                if (i < 0) \
			                { \
			                        log(DEFAULT,"main: add listen ports, epoll_ctl failed!"); \
			                        printf("ERROR: could not initialise listening sockets in epoll list. Shutting down.\n"); \
			                        Exit(ERROR); \
			                } \
			        }

#define kqueue_server_fill        log(DEFAULT,"kqueue socket engine is enabled. Filling listen list."); \
        for (count = 0; count < boundPortCount; count++) \
        { \
                struct kevent ke; \
                log(DEBUG,"kqueue: Add listening socket to events, kq=%d socket=%d",lkq,openSockfd[count]); \
                EV_SET(&ke, openSockfd[count], EVFILT_READ, EV_ADD, 0, MaxConn, NULL); \
                int i = kevent(lkq, &ke, 1, 0, 0, NULL); \
                if (i == -1) \
                { \
                        log(DEFAULT,"main: add listen ports to kqueue failed!"); \
                        printf("ERROR: could not initialise listening sockets in kqueue. Shutting down.\n"); \
                        Exit(ERROR); \
                } \
        }

#ifdef USE_EPOLL
#define engine_server_fill epoll_server_fill
#endif
#ifdef USE_KQUEUE
#define engine_server_fill kqueue_server_fill
#endif
#ifdef USE_SELECT
#define engine_server_fill select_server_fill
#endif


// what is this engine called?

#ifdef USE_KQUEUE
#define engine_name "kqueue"
#endif
#ifdef USE_SELECT
#define engine_name "select"
#endif
#ifdef USE_EPOLL
#define engine_name "epoll"
#endif

