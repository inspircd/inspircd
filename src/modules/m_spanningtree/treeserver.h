#ifndef __TREESERVER_H__
#define __TREESERVER_H__

/** Each server in the tree is represented by one class of
 * type TreeServer. A locally connected TreeServer can
 * have a class of type TreeSocket associated with it, for
 * remote servers, the TreeSocket entry will be NULL.
 * Each server also maintains a pointer to its parent
 * (NULL if this server is ours, at the top of the tree)
 * and a pointer to its "Route" (see the comments in the
 * constructors below), and also a dynamic list of pointers
 * to its children which can be iterated recursively
 * if required. Creating or deleting objects of type
 i* TreeServer automatically maintains the hash_map of
 * TreeServer items, deleting and inserting them as they
 * are created and destroyed.
 */
class TreeServer : public classbase
{
        InspIRCd* ServerInstance;               /* Creator */
        TreeServer* Parent;                     /* Parent entry */
        TreeServer* Route;                      /* Route entry */
        std::vector<TreeServer*> Children;      /* List of child objects */
        irc::string ServerName;                 /* Server's name */
        std::string ServerDesc;                 /* Server's description */
        std::string VersionString;              /* Version string or empty string */
        int UserCount;                          /* Not used in this version */
        int OperCount;                          /* Not used in this version */
        TreeSocket* Socket;                     /* For directly connected servers this points at the socket object */
        time_t NextPing;                        /* After this time, the server should be PINGed*/
        bool LastPingWasGood;                   /* True if the server responded to the last PING with a PONG */
        SpanningTreeUtilities* Utils;           /* Utility class */

 public:

        /** We don't use this constructor. Its a dummy, and won't cause any insertion
         * of the TreeServer into the hash_map. See below for the two we DO use.
         */
	TreeServer(SpanningTreeUtilities* Util, InspIRCd* Instance);

        /** We use this constructor only to create the 'root' item, Utils->TreeRoot, which
         * represents our own server. Therefore, it has no route, no parent, and
         * no socket associated with it. Its version string is our own local version.
         */
	TreeServer(SpanningTreeUtilities* Util, InspIRCd* Instance, std::string Name, std::string Desc);
	
        /** When we create a new server, we call this constructor to initialize it.
         * This constructor initializes the server's Route and Parent, and sets up
         * its ping counters so that it will be pinged one minute from now.
         */
        TreeServer(SpanningTreeUtilities* Util, InspIRCd* Instance, std::string Name, std::string Desc, TreeServer* Above, TreeSocket* Sock);

        int QuitUsers(const std::string &reason);

        /** This method is used to add the structure to the
         * hash_map for linear searches. It is only called
         * by the constructors.
         */
        void AddHashEntry();

        /** This method removes the reference to this object
         * from the hash_map which is used for linear searches.
         * It is only called by the default destructor.
         */
        void DelHashEntry();

        /** These accessors etc should be pretty self-
         * explanitory.
         */
        TreeServer* GetRoute();
        std::string GetName();
        std::string GetDesc();
        std::string GetVersion();
        void SetNextPingTime(time_t t);
        time_t NextPingTime();
        bool AnsweredLastPing();
        void SetPingFlag();
        int GetUserCount();
        void AddUserCount();
        void DelUserCount();
        int GetOperCount();
        TreeSocket* GetSocket();
        TreeServer* GetParent();
        void SetVersion(const std::string &Version);
        unsigned int ChildCount();
        TreeServer* GetChild(unsigned int n);
        void AddChild(TreeServer* Child);
        bool DelChild(TreeServer* Child);

        /** Removes child nodes of this node, and of that node, etc etc.
         * This is used during netsplits to automatically tidy up the
         * server tree. It is slow, we don't use it for much else.
         */
        bool Tidy();

        ~TreeServer();

};

#endif
