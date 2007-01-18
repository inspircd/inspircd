#ifndef __ST_MAIN__
#define __ST_MAIN__

#include "inspircd.h"
#include "modules.h"

/** If you make a change which breaks the protocol, increment this.
 * If you  completely change the protocol, completely change the number.
 *
 * IMPORTANT: If you make changes, document your changes here, without fail:
 * http://www.inspircd.org/wiki/List_of_protocol_changes_between_versions
 *
 * Failure to document your protocol changes will result in a painfully
 * painful death by pain. You have been warned.
 */
const long ProtocolVersion = 1103;


class cmd_rconnect;
class SpanningTreeUtilities;
class TimeSyncTimer;
class TreeServer;
class Link;

class ModuleSpanningTree : public Module
{
        int line;
        int NumServers;
        unsigned int max_local;
        unsigned int max_global;
        cmd_rconnect* command_rconnect;
        SpanningTreeUtilities* Utils;

 public:
        TimeSyncTimer *SyncTimer;
        ModuleSpanningTree(InspIRCd* Me);
        void ShowLinks(TreeServer* Current, userrec* user, int hops);
        int CountLocalServs();
        int CountServs();
        void HandleLinks(const char** parameters, int pcnt, userrec* user);
        void HandleLusers(const char** parameters, int pcnt, userrec* user);
        void ShowMap(TreeServer* Current, userrec* user, int depth, char matrix[128][80], float &totusers, float &totservers);
        int HandleMotd(const char** parameters, int pcnt, userrec* user);
        int HandleAdmin(const char** parameters, int pcnt, userrec* user);
        int HandleStats(const char** parameters, int pcnt, userrec* user);
        void HandleMap(const char** parameters, int pcnt, userrec* user);
        int HandleSquit(const char** parameters, int pcnt, userrec* user);
        int HandleTime(const char** parameters, int pcnt, userrec* user);
        int HandleRemoteWhois(const char** parameters, int pcnt, userrec* user);
        void DoPingChecks(time_t curtime);
        void ConnectServer(Link* x);
        void AutoConnectServers(time_t curtime);
        int HandleVersion(const char** parameters, int pcnt, userrec* user);
        int HandleConnect(const char** parameters, int pcnt, userrec* user);
        void BroadcastTimeSync();
        virtual int OnPreCommand(const std::string &command, const char** parameters, int pcnt, userrec *user, bool validated, const std::string &original_line);
        virtual void OnPostCommand(const std::string &command, const char** parameters, int pcnt, userrec *user, CmdResult result, const std::string &original_line);
        virtual void OnGetServerDescription(const std::string &servername,std::string &description);
        virtual void OnUserInvite(userrec* source,userrec* dest,chanrec* channel);
        virtual void OnPostLocalTopicChange(userrec* user, chanrec* chan, const std::string &topic);
        virtual void OnWallops(userrec* user, const std::string &text);
        virtual void OnUserNotice(userrec* user, void* dest, int target_type, const std::string &text, char status, const CUList &exempt_list);
        virtual void OnUserMessage(userrec* user, void* dest, int target_type, const std::string &text, char status, const CUList &exempt_list);
        virtual void OnBackgroundTimer(time_t curtime);
        virtual void OnUserJoin(userrec* user, chanrec* channel);
        virtual void OnChangeHost(userrec* user, const std::string &newhost);
        virtual void OnChangeName(userrec* user, const std::string &gecos);
        virtual void OnUserPart(userrec* user, chanrec* channel, const std::string &partmessage);
        virtual void OnUserConnect(userrec* user);
        virtual void OnUserQuit(userrec* user, const std::string &reason);
        virtual void OnUserPostNick(userrec* user, const std::string &oldnick);
        virtual void OnUserKick(userrec* source, userrec* user, chanrec* chan, const std::string &reason);
        virtual void OnRemoteKill(userrec* source, userrec* dest, const std::string &reason);
        virtual void OnRehash(userrec* user, const std::string &parameter);
        virtual void OnOper(userrec* user, const std::string &opertype);
        void OnLine(userrec* source, const std::string &host, bool adding, char linetype, long duration, const std::string &reason);
        virtual void OnAddGLine(long duration, userrec* source, const std::string &reason, const std::string &hostmask);
        virtual void OnAddZLine(long duration, userrec* source, const std::string &reason, const std::string &ipmask);
        virtual void OnAddQLine(long duration, userrec* source, const std::string &reason, const std::string &nickmask);
        virtual void OnAddELine(long duration, userrec* source, const std::string &reason, const std::string &hostmask);
        virtual void OnDelGLine(userrec* source, const std::string &hostmask);
        virtual void OnDelZLine(userrec* source, const std::string &ipmask);
        virtual void OnDelQLine(userrec* source, const std::string &nickmask);
        virtual void OnDelELine(userrec* source, const std::string &hostmask);
        virtual void OnMode(userrec* user, void* dest, int target_type, const std::string &text);
	virtual int OnStats(char statschar, userrec* user, string_list &results);
        virtual void OnSetAway(userrec* user);
        virtual void OnCancelAway(userrec* user);
        virtual void ProtoSendMode(void* opaque, int target_type, void* target, const std::string &modeline);
        virtual void ProtoSendMetaData(void* opaque, int target_type, void* target, const std::string &extname, const std::string &extdata);
        virtual void OnEvent(Event* event);
        virtual ~ModuleSpanningTree();
        virtual Version GetVersion();
        void Implements(char* List);
        Priority Prioritize();
};

#endif
