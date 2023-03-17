/* Modular support
 *
 * (C) 2003-2023 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 *
 * Based on the original code of Epona by Lara.
 * Based on the original code of Services by Andy Church.
 */

#include "serialize.h"

#ifndef MODULES_H
#define MODULES_H

#include "base.h"
#include "modes.h"
#include "timers.h"
#include "logger.h"
#include "extensible.h"
#include "version.h"

/** This definition is used as shorthand for the various classes
 * and functions needed to make a module loadable by the OS.
 * It defines the class factory and external AnopeInit and AnopeFini functions.
 */
#ifdef _WIN32
# define MODULE_INIT(x) \
    extern "C" DllExport Module *AnopeInit(const Anope::string &, const Anope::string &); \
    extern "C" Module *AnopeInit(const Anope::string &modname, const Anope::string &creator) \
    { \
        return new x(modname, creator); \
    } \
    BOOLEAN WINAPI DllMain(HINSTANCE, DWORD, LPVOID) \
    { \
        return TRUE; \
    } \
    extern "C" DllExport void AnopeFini(x *); \
    extern "C" void AnopeFini(x *m) \
    { \
        delete m; \
    } \
    extern "C" DllExport ModuleVersionC AnopeVersion() \
    { \
        ModuleVersionC ver; \
        ver.version_major = VERSION_MAJOR; \
        ver.version_minor = VERSION_MINOR; \
        ver.version_patch = VERSION_PATCH; \
        return ver; \
    }
#else
# define MODULE_INIT(x) \
    extern "C" DllExport Module *AnopeInit(const Anope::string &modname, const Anope::string &creator) \
    { \
        return new x(modname, creator); \
    } \
    extern "C" DllExport void AnopeFini(x *m) \
    { \
        delete m; \
    } \
    extern "C" DllExport ModuleVersionC AnopeVersion() \
    { \
        ModuleVersionC ver; \
        ver.version_major = VERSION_MAJOR; \
        ver.version_minor = VERSION_MINOR; \
        ver.version_patch = VERSION_PATCH; \
        return ver; \
    }
#endif

/**
 * This #define allows us to call a method in all
 * loaded modules in a readable simple way, e.g.:
 *
 * FOREACH_MOD(OnUserConnect, (user, exempt));
 */
#define FOREACH_MOD(ename, args) \
if (true) \
{ \
    std::vector<Module *> &_modules = ModuleManager::EventHandlers[I_ ## ename]; \
    for (std::vector<Module *>::iterator _i = _modules.begin(); _i != _modules.end();) \
    { \
        try \
        { \
            (*_i)->ename args; \
        } \
        catch (const ModuleException &modexcept) \
        { \
            Log() << "Exception caught: " << modexcept.GetReason(); \
        } \
        catch (const NotImplementedException &) \
        { \
            _i = _modules.erase(_i); \
            continue; \
        } \
        ++_i; \
    } \
} \
else \
    static_cast<void>(0)

/**
 * This define is similar to the one above but returns a result.
 * The first module to return a result other than EVENT_CONTINUE is the value to be accepted,
 * and any modules after are ignored. This is used like:
 *
 * EventReturn MOD_RESULT;
 * FOREACH_RESULT(OnUserConnect, MOD_RESULT, (user, exempt));
 */
#define FOREACH_RESULT(ename, ret, args) \
if (true) \
{ \
    ret = EVENT_CONTINUE; \
    std::vector<Module *> &_modules = ModuleManager::EventHandlers[I_ ## ename]; \
    for (std::vector<Module *>::iterator _i = _modules.begin(); _i != _modules.end();) \
    { \
        try \
        { \
            EventReturn res = (*_i)->ename args; \
            if (res != EVENT_CONTINUE) \
            { \
                ret = res; \
                break; \
            } \
        } \
        catch (const ModuleException &modexcept) \
        { \
            Log() << "Exception caught: " << modexcept.GetReason(); \
        } \
        catch (const NotImplementedException &) \
        { \
            _i = _modules.erase(_i); \
            continue; \
        } \
        ++_i; \
    } \
} \
else \
    static_cast<void>(0)


/** Possible return types from events.
 */
enum EventReturn {
    EVENT_STOP,
    EVENT_CONTINUE,
    EVENT_ALLOW
};

enum ModuleReturn {
    MOD_ERR_OK,
    MOD_ERR_PARAMS,
    MOD_ERR_EXISTS,
    MOD_ERR_NOEXIST,
    MOD_ERR_NOLOAD,
    MOD_ERR_UNKNOWN,
    MOD_ERR_FILE_IO,
    MOD_ERR_EXCEPTION,
    MOD_ERR_VERSION
};

/** Priority types which can be returned from Module::Prioritize()
 */
enum Priority { PRIORITY_FIRST, PRIORITY_DONTCARE, PRIORITY_LAST, PRIORITY_BEFORE, PRIORITY_AFTER };
/* Module types, in the order in which they are unloaded. The order these are in is IMPORTANT */
enum {
    MT_BEGIN,
    /* Module is 3rd party. All 3rd party modules should set this. Mutually exclusive to VENDOR. */
    THIRD = 1 << 0,
    /* A vendor module, which is made and shipped by Anope. Mutually exclusive to THIRD. */
    VENDOR = 1 << 1,
    /* Extra module not required for standard use. Probably requires external dependencies.
     * This module does something extreme enough that we want it to show in the default /os modlist command
     */
    EXTRA = 1 << 2,
    /* Module provides access to a database */
    DATABASE = 1 << 3,
    /* Module provides encryption */
    ENCRYPTION = 1 << 4,
    /* Module provides a pseudoclient */
    PSEUDOCLIENT = 1 << 5,
    /* Module provides IRCd protocol support */
    PROTOCOL = 1 << 6,
    MT_END = 1 << 7
};
typedef unsigned short ModType;

struct ModuleVersionC {
    int version_major, version_minor, version_patch;
};

/** Returned by Module::GetVersion, used to see what version of Anope
 * a module is compiled against.
 */
class ModuleVersion {
  private:
    int version_major;
    int version_minor;
    int version_patch;

  public:
    ModuleVersion(const ModuleVersionC &);

    /** Get the major version of Anope this was built against
     * @return The major version
     */
    int GetMajor() const;

    /** Get the minor version of Anope this was built against
     * @return The minor version
     */
    int GetMinor() const;

    /** Get the patch version this was built against
     * @return The patch version
     */
    int GetPatch() const;
};

class NotImplementedException : public CoreException { };

/** Every module in Anope is actually a class.
 */
class CoreExport Module : public Extensible {
  private:
    bool permanent;
  public:
    /** The module name (e.g. os_modload)
     */
    Anope::string name;

    /** What type this module is
     */
    ModType type;

    /** The temporary path/filename
     */
    Anope::string filename;

    /** Handle for this module, obtained from dlopen()
     */
    void *handle;

    /** Time this module was created
     */
    time_t created;

    /** Version of this module
     */
    Anope::string version;

    /** Author of the module
     */
    Anope::string author;

    /** Creates and initialises a new module.
     * @param modname The module name
     * @param loadernick The nickname of the user loading the module.
     * @param type The module type
     */
    Module(const Anope::string &modname, const Anope::string &loadernick,
           ModType type = THIRD);

    /** Destroys a module, freeing resources it has allocated.
     */
    virtual ~Module();

    /** Toggles the permanent flag on a module. If a module is permanent,
     * then it may not be unloaded.
     *
     * Naturally, this setting should be used sparingly!
     *
     * @param state True if this module should be permanent, false else.
     */
    void SetPermanent(bool state);

    /** Retrieves whether or not a given module is permanent.
     * @return true if the module is permanent, false else.
     */
    bool GetPermanent() const;

    /** Set the modules version info.
     * @param version the version of the module
     */
    void SetVersion(const Anope::string &version);

    /** Set the modules author info
     * @param author the author of the module
     */
    void SetAuthor(const Anope::string &author);

    virtual void Prioritize();

    /* Everything below here are events. Modules must ModuleManager::Attach to these events
     * before they will be called.
     */

    /** Called on startup after database load, but before
     * connecting to the uplink.
     */
    virtual void OnPostInit() {
        throw NotImplementedException();
    }

    /** Called before a user has been kicked from a channel.
     * @param source The kicker
     * @param cu The user, channel, and status of the user being kicked
     * @param kickmsg The reason for the kick.
     */
    virtual void OnPreUserKicked(const MessageSource &source, ChanUserContainer *cu,
                                 const Anope::string &kickmsg) {
        throw NotImplementedException();
    }

    /** Called when a user has been kicked from a channel.
     * @param source The kicker
     * @param target The user being kicked
     * @param channel The channel the user was kicked from, which may no longer exist
     * @param status The status the kicked user had on the channel before they were kicked
     * @param kickmsg The reason for the kick.
     */
    virtual void OnUserKicked(const MessageSource &source, User *target,
                              const Anope::string &channel, ChannelStatus &status,
                              const Anope::string &kickmsg) {
        throw NotImplementedException();
    }

    /** Called when Services' configuration is being (re)loaded.
     * @param conf The config that is being built now and will replace the global Config object
     * @throws A ConfigException to abort the config (re)loading process.
     */
    virtual void OnReload(Configuration::Conf *conf) {
        throw NotImplementedException();
    }

    /** Called before a bot is assigned to a channel.
     * @param sender The user assigning the bot
     * @param ci The channel the bot is to be assigned to.
     * @param bi The bot being assigned.
     * @return EVENT_CONTINUE to let other modules decide, EVENT_STOP to deny the assign.
     */
    virtual EventReturn OnPreBotAssign(User *sender, ChannelInfo *ci, BotInfo *bi) {
        throw NotImplementedException();
    }

    /** Called when a bot is assigned ot a channel
     */
    virtual void OnBotAssign(User *sender, ChannelInfo *ci, BotInfo *bi) {
        throw NotImplementedException();
    }

    /** Called before a bot is unassigned from a channel.
     * @param sender The user unassigning the bot
     * @param ci The channel the bot is being removed from
     * @return EVENT_CONTINUE to let other modules decide, EVENT_STOP to deny the unassign.
     */
    virtual EventReturn OnBotUnAssign(User *sender, ChannelInfo *ci) {
        throw NotImplementedException();
    }

    /** Called when a new user connects to the network.
     * @param u The connecting user.
     * @param exempt set to true/is true if the user should be excepted from bans etc
     */
    virtual void OnUserConnect(User *u, bool &exempt) {
        throw NotImplementedException();
    }

    /** Called when a new server connects to the network.
     * @param s The server that has connected to the network
     */
    virtual void OnNewServer(Server *s) {
        throw NotImplementedException();
    }

    /** Called after a user changed the nick
     * @param u The user.
     * @param oldnick The old nick of the user
     */
    virtual void OnUserNickChange(User *u, const Anope::string &oldnick) {
        throw NotImplementedException();
    }

    /** Called when someone uses the generic/help command
     * @param source Command source
     * @param params Params
     * @return EVENT_STOP to stop processing
     */
    virtual EventReturn OnPreHelp(CommandSource &source,
                                  const std::vector<Anope::string> &params) {
        throw NotImplementedException();
    }

    /** Called when someone uses the generic/help command
     * @param source Command source
     * @param params Params
     */
    virtual void OnPostHelp(CommandSource &source,
                            const std::vector<Anope::string> &params) {
        throw NotImplementedException();
    }

    /** Called before a command is due to be executed.
     * @param source The source of the command
     * @param command The command the user is executing
     * @param params The parameters the user is sending
     * @return EVENT_CONTINUE to let other modules decide, EVENT_STOP to halt the command and not process it
     */
    virtual EventReturn OnPreCommand(CommandSource &source, Command *command,
                                     std::vector<Anope::string> &params) {
        throw NotImplementedException();
    }

    /** Called after a command has been executed.
     * @param source The source of the command
     * @param command The command the user executed
     * @param params The parameters the user sent
     */
    virtual void OnPostCommand(CommandSource &source, Command *command,
                               const std::vector<Anope::string> &params) {
        throw NotImplementedException();
    }

    /** Called when the databases are saved
     */
    virtual void OnSaveDatabase() {
        throw NotImplementedException();
    }

    /** Called when the databases are loaded
     * @return EVENT_CONTINUE to let other modules continue loading, EVENT_STOP to stop
     */
    virtual EventReturn OnLoadDatabase() {
        throw NotImplementedException();
    }

    /** Called when anope needs to check passwords against encryption
     *  see src/encrypt.c for detailed informations
     */
    virtual EventReturn OnEncrypt(const Anope::string &src, Anope::string &dest) {
        throw NotImplementedException();
    }
    virtual EventReturn OnDecrypt(const Anope::string &hashm,
                                  const Anope::string &src, Anope::string &dest) {
        throw NotImplementedException();
    }

    /** Called on fantasy command
     * @param source The source of the command
     * @param c The command
     * @param ci The channel it's being used in
     * @param params The params
     * @return EVENT_STOP to halt processing and not run the command, EVENT_ALLOW to allow the command to be executed
     */
    virtual EventReturn OnBotFantasy(CommandSource &source, Command *c,
                                     ChannelInfo *ci, const std::vector<Anope::string> &params) {
        throw NotImplementedException();
    }

    /** Called on fantasy command without access
     * @param source The source of the command
     * @param c The command
     * @param ci The channel it's being used in
     * @param params The params
     * @return EVENT_STOP to halt processing and not run the command, EVENT_ALLOW to allow the command to be executed
     */
    virtual EventReturn OnBotNoFantasyAccess(CommandSource &source, Command *c,
            ChannelInfo *ci, const std::vector<Anope::string> &params) {
        throw NotImplementedException();
    }

    /** Called when a bot places a ban
     * @param u User being banned
     * @param ci Channel the ban is placed on
     * @param mask The mask being banned
     */
    virtual void OnBotBan(User *u, ChannelInfo *ci, const Anope::string &mask) {
        throw NotImplementedException();
    }

    /** Called before a badword is added to the badword list
     * @param ci The channel
     * @param bw The badword
     */
    virtual void OnBadWordAdd(ChannelInfo *ci, const BadWord *bw) {
        throw NotImplementedException();
    }

    /** Called before a badword is deleted from a channel
     * @param ci The channel
     * @param bw The badword
     */
    virtual void OnBadWordDel(ChannelInfo *ci, const BadWord *bw) {
        throw NotImplementedException();
    }

    /** Called when a bot is created or destroyed
     */
    virtual void OnCreateBot(BotInfo *bi) {
        throw NotImplementedException();
    }
    virtual void OnDelBot(BotInfo *bi) {
        throw NotImplementedException();
    }

    /** Called before a bot kicks a user
     * @param bi The bot sending the kick
     * @param c The channel the user is being kicked on
     * @param u The user being kicked
     * @param reason The reason
     * @return EVENT_CONTINUE to let other modules decide, EVENT_STOP to halt the command and not process it
     */
    virtual EventReturn OnBotKick(BotInfo *bi, Channel *c, User *u,
                                  const Anope::string &reason) {
        throw NotImplementedException();
    }

    /** Called before a user parts a channel
     * @param u The user
     * @param c The channel
     */
    virtual void OnPrePartChannel(User *u, Channel *c) {}

    /** Called when a user parts a channel
     * @param u The user
     * @param c The channel, may be NULL if the channel no longer exists
     * @param channel The channel name
     * @param msg The part reason
     */
    virtual void OnPartChannel(User *u, Channel *c, const Anope::string &channel,
                               const Anope::string &msg) {
        throw NotImplementedException();
    }

    /** Called when a user leaves a channel.
     * From either parting, being kicked, or quitting/killed!
     * @param u The user
     * @param c The channel
     */
    virtual void OnLeaveChannel(User *u, Channel *c) {
        throw NotImplementedException();
    }

    /** Called after a user joins a channel
     * If this event triggers the user is allowed to be in the channel, and will
     * not be kicked for restricted/akick/forbidden, etc. If you want to kick the user,
     * use the CheckKick event instead.
     * @param u The user
     * @param channel The channel
     */
    virtual void OnJoinChannel(User *u, Channel *c) {
        throw NotImplementedException();
    }

    /** Called when a new topic is set
     * @param source The user changing the topic, if any
     * @param c The channel
     * @param setter The user who set the new topic, if there is no source
     * @param topic The new topic
     */
    virtual void OnTopicUpdated(User *source, Channel *c, const Anope::string &user,
                                const Anope::string &topic) {
        throw NotImplementedException();
    }

    /** Called before a channel expires
     * @param ci The channel
     * @param expire Set to true to allow the chan to expire
     */
    virtual void OnPreChanExpire(ChannelInfo *ci, bool &expire) {
        throw NotImplementedException();
    }

    /** Called before a channel expires
     * @param ci The channel
     */
    virtual void OnChanExpire(ChannelInfo *ci) {
        throw NotImplementedException();
    }

    /** Called before Anope connects to its uplink
     */
    virtual void OnPreServerConnect() {
        throw NotImplementedException();
    }

    /** Called when Anope connects to its uplink
     */
    virtual void OnServerConnect() {
        throw NotImplementedException();
    }

    /** Called when we are almost done synching with the uplink, just before we send the EOB
     */
    virtual void OnPreUplinkSync(Server *serv) {
        throw NotImplementedException();
    }

    /** Called when Anope disconnects from its uplink, before it tries to reconnect
     */
    virtual void OnServerDisconnect() {
        throw NotImplementedException();
    }

    /** Called when services restart
    */
    virtual void OnRestart() {
        throw NotImplementedException();
    }

    /** Called when services shutdown
     */
    virtual void OnShutdown() {
        throw NotImplementedException();
    }

    /** Called before a nick expires
     * @param na The nick
     * @param expire Set to true to allow the nick to expire
     */
    virtual void OnPreNickExpire(NickAlias *na, bool &expire) {
        throw NotImplementedException();
    }

    /** Called when a nick drops
     * @param na The nick
     */
    virtual void OnNickExpire(NickAlias *na) {
        throw NotImplementedException();
    }

    /** Called when defcon level changes
     * @param level The level
     */
    virtual void OnDefconLevel(int level) {
        throw NotImplementedException();
    }

    /** Called after an exception has been added
     * @param ex The exception
     * @return EVENT_CONTINUE to let other modules decide, EVENT_STOP to halt the command and not process it
     */
    virtual EventReturn OnExceptionAdd(Exception *ex) {
        throw NotImplementedException();
    }

    /** Called before an exception is deleted
     * @param source The source deleting it
     * @param ex The exception
     */
    virtual void OnExceptionDel(CommandSource &source, Exception *ex) {
        throw NotImplementedException();
    }

    /** Called before a XLine is added
     * @param source The source of the XLine
     * @param x The XLine
     * @param xlm The xline manager it was added to
     * @return EVENT_CONTINUE to let other modules decide, EVENT_STOP to halt the command and not process it
     */
    virtual EventReturn OnAddXLine(CommandSource &source, const XLine *x,
                                   XLineManager *xlm) {
        throw NotImplementedException();
    }

    /** Called before a XLine is deleted
     * @param source The source of the XLine
     * @param x The XLine
     * @param xlm The xline manager it was deleted from
     */
    virtual void OnDelXLine(CommandSource &source, const XLine *x,
                            XLineManager *xlm) {
        throw NotImplementedException();
    }

    /** Called when a user is checked for whether they are a services oper
     * @param u The user
     * @return EVENT_ALLOW to allow, anything else to deny
     */
    virtual EventReturn IsServicesOper(User *u) {
        throw NotImplementedException();
    }

    /** Called when a server quits
     * @param server The server
     */
    virtual void OnServerQuit(Server *server) {
        throw NotImplementedException();
    }

    /** Called when a user quits, or is killed
     * @param u The user
     * @param msg The quit message
     */
    virtual void OnUserQuit(User *u, const Anope::string &msg) {
        throw NotImplementedException();
    }

    /** Called when a user is quit, before and after being internally removed from
     * This is different from OnUserQuit, which takes place at the time of the quit.
     * This happens shortly after when all message processing is finished.
     * all lists (channels, user list, etc)
     * @param u The user
     */
    virtual void OnPreUserLogoff(User *u) {
        throw NotImplementedException();
    }
    virtual void OnPostUserLogoff(User *u) {
        throw NotImplementedException();
    }

    /** Called when a new bot is made
     * @param bi The bot
     */
    virtual void OnBotCreate(BotInfo *bi) {
        throw NotImplementedException();
    }

    /** Called when a bot is changed
     * @param bi The bot
     */
    virtual void OnBotChange(BotInfo *bi) {
        throw NotImplementedException();
    }

    /** Called when a bot is deleted
     * @param bi The bot
     */
    virtual void OnBotDelete(BotInfo *bi) {
        throw NotImplementedException();
    }

    /** Called after an access entry is deleted from a channel
     * @param ci The channel
     * @param source The source of the command
     * @param access The access entry that was removed
     */
    virtual void OnAccessDel(ChannelInfo *ci, CommandSource &source,
                             ChanAccess *access) {
        throw NotImplementedException();
    }

    /** Called when access is added
     * @param ci The channel
     * @param source The source of the command
     * @param access The access changed
     */
    virtual void OnAccessAdd(ChannelInfo *ci, CommandSource &source,
                             ChanAccess *access) {
        throw NotImplementedException();
    }

    /** Called when the access list is cleared
     * @param ci The channel
     * @param u The user who cleared the access
     */
    virtual void OnAccessClear(ChannelInfo *ci, CommandSource &source) {
        throw NotImplementedException();
    }

    /** Called when a level for a channel is changed
     * @param source The source of the command
     * @param ci The channel the level was changed on
     * @param priv The privilege changed
     * @param what The new level
     */
    virtual void OnLevelChange(CommandSource &source, ChannelInfo *ci,
                               const Anope::string &priv, int16_t what) {
        throw NotImplementedException();
    }

    /** Called right before a channel is dropped
     * @param source The user dropping the channel
     * @param ci The channel
     */
    virtual EventReturn OnChanDrop(CommandSource &source, ChannelInfo *ci) {
        throw NotImplementedException();
    }

    /** Called when a channel is registered
     * @param ci The channel
     */
    virtual void OnChanRegistered(ChannelInfo *ci) {
        throw NotImplementedException();
    }

    /** Called when a channel is suspended
     * @param ci The channel
     */
    virtual void OnChanSuspend(ChannelInfo *ci) {
        throw NotImplementedException();
    }

    /** Called when a channel is unsuspended
     * @param ci The channel
     */
    virtual void OnChanUnsuspend(ChannelInfo *ci) {
        throw NotImplementedException();
    }

    /** Called when a channel is being created, for any reason
     * @param ci The channel
     */
    virtual void OnCreateChan(ChannelInfo *ci) {
        throw NotImplementedException();
    }

    /** Called when a channel is being deleted, for any reason
     * @param ci The channel
     */
    virtual void OnDelChan(ChannelInfo *ci) {
        throw NotImplementedException();
    }

    /** Called when a new channel is created
     * Note that this channel may not be introduced to the uplink at this point.
     * @param c The channel
     */
    virtual void OnChannelCreate(Channel *c) {
        throw NotImplementedException();
    }

    /** Called when a channel is deleted
     * @param c The channel
     */
    virtual void OnChannelDelete(Channel *c) {
        throw NotImplementedException();
    }

    /** Called after adding an akick to a channel
     * @param source The source of the command
     * @param ci The channel
     * @param ak The akick
     */
    virtual void OnAkickAdd(CommandSource &source, ChannelInfo *ci,
                            const AutoKick *ak) {
        throw NotImplementedException();
    }

    /** Called before removing an akick from a channel
     * @param source The source of the command
     * @param ci The channel
     * @param ak The akick
     */
    virtual void OnAkickDel(CommandSource &source, ChannelInfo *ci,
                            const AutoKick *ak) {
        throw NotImplementedException();
    }

    /** Called after a user join a channel when we decide whether to kick them or not
     * @param u The user
     * @param c The channel
     * @param kick Set to true to kick
     * @param mask The mask to ban, if any
     * @param reason The reason for the kick
     * @return EVENT_STOP to prevent the user from joining by kicking/banning the user
     */
    virtual EventReturn OnCheckKick(User *u, Channel *c, Anope::string &mask,
                                    Anope::string &reason) {
        throw NotImplementedException();
    }

    /** Called when a user requests info for a channel
     * @param source The user requesting info
     * @param ci The channel the user is requesting info for
     * @param info Data to show the user requesting information
     * @param show_hidden true if we should show the user everything
     */
    virtual void OnChanInfo(CommandSource &source, ChannelInfo *ci,
                            InfoFormatter &info, bool show_hidden) {
        throw NotImplementedException();
    }

    /** Checks if access has the channel privilege 'priv'.
     * @param access THe access struct
     * @param priv The privilege being checked for
     * @return EVENT_ALLOW for yes, EVENT_STOP to stop all processing
     */
    virtual EventReturn OnCheckPriv(const ChanAccess *access,
                                    const Anope::string &priv) {
        throw NotImplementedException();
    }

    /** Check whether an access group has a privilege
     * @param group The group
     * @param priv The privilege
     * @return MOD_ALLOW to allow, MOD_STOP to stop
     */
    virtual EventReturn OnGroupCheckPriv(const AccessGroup *group,
                                         const Anope::string &priv) {
        throw NotImplementedException();
    }

    /** Called when a nick is dropped
     * @param source The source of the command
     * @param na The nick
     */
    virtual void OnNickDrop(CommandSource &source, NickAlias *na) {
        throw NotImplementedException();
    }

    /** Called when a user groups their nick
     * @param u The user grouping
     * @param target The target they're grouping to
     */
    virtual void OnNickGroup(User *u, NickAlias *target) {
        throw NotImplementedException();
    }

    /** Called when a user identifies to a nick
     * @param u The user
     */
    virtual void OnNickIdentify(User *u) {
        throw NotImplementedException();
    }

    /** Called when a user is logged into an account
     * @param u The user
     */
    virtual void OnUserLogin(User *u) {
        throw NotImplementedException();
    }

    /** Called when a nick logs out
     * @param u The nick
     */
    virtual void OnNickLogout(User *u) {
        throw NotImplementedException();
    }

    /** Called when a nick is registered
     * @param user The user registering the nick, of any
     * @param The nick
     * @param pass The password of the newly registered nick
     */
    virtual void OnNickRegister(User *user, NickAlias *na,
                                const Anope::string &pass) {
        throw NotImplementedException();
    }

    /** Called when a nick is confirmed. This will never be called if registration confirmation is not enabled.
     * @param user The user confirming the nick
     * @param The account being confirmed
     */
    virtual void OnNickConfirm(User *user, NickCore *) {
        throw NotImplementedException();
    }

    /** Called when a nick is suspended
     * @param na The nick alias
     */
    virtual void OnNickSuspend(NickAlias *na) {
        throw NotImplementedException();
    }

    /** Called when a nick is unsuspended
     * @param na The nick alias
     */
    virtual void OnNickUnsuspended(NickAlias *na) {
        throw NotImplementedException();
    }

    /** Called on delnick()
     * @ param na pointer to the nickalias
     */
    virtual void OnDelNick(NickAlias *na) {
        throw NotImplementedException();
    }

    /** Called when a nickcore is created
     * @param nc The nickcore
     */
    virtual void OnNickCoreCreate(NickCore *nc) {
        throw NotImplementedException();
    }

    /** Called on delcore()
     * @param nc pointer to the NickCore
     */
    virtual void OnDelCore(NickCore *nc) {
        throw NotImplementedException();
    }

    /** Called on change_core_display()
     * @param nc pointer to the NickCore
     * @param newdisplay the new display
     */
    virtual void OnChangeCoreDisplay(NickCore *nc,
                                     const Anope::string &newdisplay) {
        throw NotImplementedException();
    }

    /** called from NickCore::ClearAccess()
     * @param nc pointer to the NickCore
     */
    virtual void OnNickClearAccess(NickCore *nc) {
        throw NotImplementedException();
    }

    /** Called when a user adds an entry to their access list
     * @param nc The nick
     * @param entry The entry
     */
    virtual void OnNickAddAccess(NickCore *nc, const Anope::string &entry) {
        throw NotImplementedException();
    }

    /** Called from NickCore::EraseAccess()
     * @param nc pointer to the NickCore
     * @param entry The access mask
     */
    virtual void OnNickEraseAccess(NickCore *nc, const Anope::string &entry) {
        throw NotImplementedException();
    }

    /** called from NickCore::ClearCert()
     * @param nc pointer to the NickCore
     */
    virtual void OnNickClearCert(NickCore *nc) {
        throw NotImplementedException();
    }

    /** Called when a user adds an entry to their cert list
     * @param nc The nick
     * @param entry The entry
     */
    virtual void OnNickAddCert(NickCore *nc, const Anope::string &entry) {
        throw NotImplementedException();
    }

    /** Called from NickCore::EraseCert()
     * @param nc pointer to the NickCore
     * @param entry The fingerprint
     */
    virtual void OnNickEraseCert(NickCore *nc, const Anope::string &entry) {
        throw NotImplementedException();
    }

    /** Called when a user requests info for a nick
     * @param source The user requesting info
     * @param na The nick the user is requesting info from
     * @param info Data to show the user requesting information
     * @param show_hidden true if we should show the user everything
     */
    virtual void OnNickInfo(CommandSource &source, NickAlias *na,
                            InfoFormatter &info, bool show_hidden) {
        throw NotImplementedException();
    }

    /** Called when a user uses botserv/info on a bot or channel.
     */
    virtual void OnBotInfo(CommandSource &source, BotInfo *bi, ChannelInfo *ci,
                           InfoFormatter &info) {
        throw NotImplementedException();
    }

    /** Check whether a username and password is correct
     * @param u The user trying to identify, if applicable.
     * @param req The login request
     */
    virtual void OnCheckAuthentication(User *u, IdentifyRequest *req) {
        throw NotImplementedException();
    }

    /** Called when a user does /ns update
     * @param u The user
     */
    virtual void OnNickUpdate(User *u) {
        throw NotImplementedException();
    }

    /** Called when we get informed about a users SSL fingerprint
     *  when we call this, the fingerprint should already be stored in the user struct
     * @param u pointer to the user
     */
    virtual void OnFingerprint(User *u) {
        throw NotImplementedException();
    }

    /** Called when a user becomes (un)away
     * @param message The message, is .empty() if unaway
     */
    virtual void OnUserAway(User *u, const Anope::string &message) {
        throw NotImplementedException();
    }

    /** Called when a user invites one of our users to a channel
     * @param source The user doing the inviting
     * @param c The channel the user is inviting to
     * @param targ The user being invited
     */
    virtual void OnInvite(User *source, Channel *c, User *targ) {
        throw NotImplementedException();
    }

    /** Called when a vhost is deleted
     * @param na The nickalias of the vhost
     */
    virtual void OnDeleteVhost(NickAlias *na) {
        throw NotImplementedException();
    }

    /** Called when a vhost is set
     * @param na The nickalias of the vhost
     */
    virtual void OnSetVhost(NickAlias *na) {
        throw NotImplementedException();
    }

    /** Called when a users host changes
     * @param u The user
     */
    virtual void OnSetDisplayedHost(User *) {
        throw NotImplementedException();
    }

    /** Called when a memo is sent
     * @param source The source of the memo
     * @param target The target of the memo
     * @param mi Memo info for target
     * @param m The memo
     */
    virtual void OnMemoSend(const Anope::string &source,
                            const Anope::string &target, MemoInfo *mi, Memo *m) {
        throw NotImplementedException();
    }

    /** Called when a memo is deleted
     * @param target The target the memo is being deleted from (nick or channel)
     * @param mi The memo info
     * @param m The memo
     */
    virtual void OnMemoDel(const Anope::string &target, MemoInfo *mi,
                           const Memo *m) {
        throw NotImplementedException();
    }

    /** Called when a mode is set on a channel
     * @param c The channel
     * @param setter The user or server that is setting the mode
     * @param mode The mode
     * @param param The mode param, if there is one
     * @return EVENT_STOP to make mlock/secureops etc checks not happen
     */
    virtual EventReturn OnChannelModeSet(Channel *c, MessageSource &setter,
                                         ChannelMode *mode, const Anope::string &param) {
        throw NotImplementedException();
    }

    /** Called when a mode is unset on a channel
     * @param c The channel
     * @param setter The user or server that is unsetting the mode
     * @param mode The mode
     * @param param The mode param, if there is one
     * @return EVENT_STOP to make mlock/secureops etc checks not happen
     */
    virtual EventReturn OnChannelModeUnset(Channel *c, MessageSource &setter,
                                           ChannelMode *mode, const Anope::string &param) {
        throw NotImplementedException();
    }

    /** Called when a mode is set on a user
     * @param setter who/what is setting the mode
     * @param u The user
     * @param mname The mode name
     */
    virtual void OnUserModeSet(const MessageSource &setter, User *u,
                               const Anope::string &mname) {
        throw NotImplementedException();
    }

    /** Called when a mode is unset from a user
     * @param setter who/what is setting the mode
     * @param u The user
     * @param mname The mode name
     */
    virtual void OnUserModeUnset(const MessageSource &setter, User *u,
                                 const Anope::string &mname) {
        throw NotImplementedException();
    }

    /** Called when a channel mode is introduced into Anope
     * @param cm The mode
     */
    virtual void OnChannelModeAdd(ChannelMode *cm) {
        throw NotImplementedException();
    }

    /** Called when a user mode is introduced into Anope
     * @param um The mode
     */
    virtual void OnUserModeAdd(UserMode *um) {
        throw NotImplementedException();
    }

    /** Called when a mode is about to be mlocked
     * @param ci The channel the mode is being locked on
     * @param lock The mode lock
     * @return EVENT_CONTINUE to let other modules decide, EVENT_STOP to deny the mlock.
     */
    virtual EventReturn OnMLock(ChannelInfo *ci, ModeLock *lock) {
        throw NotImplementedException();
    }

    /** Called when a mode is about to be unlocked
     * @param ci The channel the mode is being unlocked from
     * @param lock The mode lock
     * @return EVENT_CONTINUE to let other modules decide, EVENT_STOP to deny the mlock.
     */
    virtual EventReturn OnUnMLock(ChannelInfo *ci, ModeLock *lock) {
        throw NotImplementedException();
    }

    /** Called after a module is loaded
     * @param u The user loading the module, can be NULL
     * @param m The module
     */
    virtual void OnModuleLoad(User *u, Module *m) {
        throw NotImplementedException();
    }

    /** Called before a module is unloaded
     * @param u The user, can be NULL
     * @param m The module
     */
    virtual void OnModuleUnload(User *u, Module *m) {
        throw NotImplementedException();
    }

    /** Called when a server is synced
     * @param s The server, can be our uplink server
     */
    virtual void OnServerSync(Server *s) {
        throw NotImplementedException();
    }

    /** Called when we sync with our uplink
     * @param s Our uplink
     */
    virtual void OnUplinkSync(Server *s) {
        throw NotImplementedException();
    }

    /** Called when we receive a PRIVMSG for one of our clients
     * @param u The user sending the PRIVMSG
     * @param bi The target of the PRIVMSG
     * @param message The message
     * @return EVENT_STOP to halt processing
     */
    virtual EventReturn OnBotPrivmsg(User *u, BotInfo *bi, Anope::string &message) {
        throw NotImplementedException();
    }

    /** Called when we receive a NOTICE for one of our clients
     * @param u The user sending the NOTICE
     * @param bi The target of the NOTICE
     * @param message The message
     */
    virtual void OnBotNotice(User *u, BotInfo *bi, Anope::string &message) {
        throw NotImplementedException();
    }

    /** Called when we receive a PRIVMSG for a registered channel we are in
     * @param u The source of the message
     * @param c The channel
     * @param msg The message
     */
    virtual void OnPrivmsg(User *u, Channel *c, Anope::string &msg) {
        throw NotImplementedException();
    }

    /** Called when a message is logged
     * @param l The log message
     */
    virtual void OnLog(Log *l) {
        throw NotImplementedException();
    }

    /** Called when a log message is actually logged to a given log info
     * The message has already passed validation checks by the LogInfo
     * @param li The loginfo whee the message is being logged
     * @param l The log message
     * @param msg The final formatted message, derived from 'l'
     */
    virtual void OnLogMessage(LogInfo *li, const Log *l, const Anope::string &msg) {
        throw NotImplementedException();
    }

    /** Called when a DNS request (question) is received.
     * @param req The dns request
     * @param reply The reply that will be sent
     */
    virtual void OnDnsRequest(DNS::Query &req, DNS::Query *reply) {
        throw NotImplementedException();
    }

    /** Called when a channels modes are being checked to see if they are allowed,
     * mostly to ensure mlock/+r are set.
     * @param c The channel
     */
    virtual void OnCheckModes(Reference<Channel> &c) {
        throw NotImplementedException();
    }

    /** Called when a channel is synced.
     * Channels are synced after a sjoin is finished processing
     * for a newly created channel to set the correct modes, topic,
     * set.
     */
    virtual void OnChannelSync(Channel *c) {
        throw NotImplementedException();
    }

    /** Called to set the correct modes on the user on the given channel
     * @param user The user
     * @param chan The channel
     * @param access The user's access on the channel
     * @param give_modes If giving modes is desired
     * @param take_modes If taking modes is desired
     */
    virtual void OnSetCorrectModes(User *user, Channel *chan, AccessGroup &access,
                                   bool &give_modes, bool &take_modes) {
        throw NotImplementedException();
    }

    virtual void OnSerializeCheck(Serialize::Type *) {
        throw NotImplementedException();
    }
    virtual void OnSerializableConstruct(Serializable *) {
        throw NotImplementedException();
    }
    virtual void OnSerializableDestruct(Serializable *) {
        throw NotImplementedException();
    }
    virtual void OnSerializableUpdate(Serializable *) {
        throw NotImplementedException();
    }
    virtual void OnSerializeTypeCreate(Serialize::Type *) {
        throw NotImplementedException();
    }

    /** Called when a chanserv/set command is used
     * @param source The source of the command
     * @param cmd The command
     * @param ci The channel the command was used on
     * @param setting The setting passed to the command. Probably ON/OFF.
     * @return EVENT_ALLOW to bypass access checks, EVENT_STOP to halt immediately.
     */
    virtual EventReturn OnSetChannelOption(CommandSource &source, Command *cmd,
                                           ChannelInfo *ci, const Anope::string &setting) {
        throw NotImplementedException();
    }

    /** Called when a nickserv/set command is used.
     * @param source The source of the command
     * @param cmd The command
     * @param nc The nickcore being modifed
     * @param setting The setting passed to the command. Probably ON/OFF.
     * @return EVENT_STOP to halt immediately
     */
    virtual EventReturn OnSetNickOption(CommandSource &source, Command *cmd,
                                        NickCore *nc, const Anope::string &setting) {
        throw NotImplementedException();
    }

    /** Called whenever a message is received from the uplink
     * @param source The source of the message
     * @param command The command being executed
     * @param params Parameters
     * @return EVENT_STOP to prevent the protocol module from processing this message
     */
    virtual EventReturn OnMessage(MessageSource &source, Anope::string &command,
                                  std::vector<Anope::string> &param) {
        throw NotImplementedException();
    }

    /** Called to determine if a channel mode can be set by a user
     * @param u The user
     * @param cm The mode
     */
    virtual EventReturn OnCanSet(User *u, const ChannelMode *cm) {
        throw NotImplementedException();
    }

    virtual EventReturn OnCheckDelete(Channel *) {
        throw NotImplementedException();
    }

    /** Called every options:expiretimeout seconds. Should be used to expire nicks,
     * channels, etc.
     */
    virtual void OnExpireTick() {
        throw NotImplementedException();
    }

    /** Called when a nick is validated. That is, to determine if a user is permitted
     * to be on the given nick.
     * @param u The user
     * @param na The nick they are on
     * @return EVENT_STOP to force the user off of the nick
     */
    virtual EventReturn OnNickValidate(User *u, NickAlias *na) {
        throw NotImplementedException();
    }

    /** Called when a certain user has to be unbanned on a certain channel.
     * May be used to send protocol-specific messages.
     * @param u The user to be unbanned
     * @param c The channel that user has to be unbanned on
     */
    virtual void OnChannelUnban(User *u, ChannelInfo *ci) {
        throw NotImplementedException();
    }
};

enum Implementation {
    I_OnPostInit,
    I_OnPreUserKicked, I_OnUserKicked, I_OnReload, I_OnPreBotAssign, I_OnBotAssign, I_OnBotUnAssign, I_OnUserConnect,
    I_OnNewServer, I_OnUserNickChange, I_OnPreHelp, I_OnPostHelp, I_OnPreCommand, I_OnPostCommand, I_OnSaveDatabase,
    I_OnLoadDatabase, I_OnEncrypt, I_OnDecrypt, I_OnBotFantasy, I_OnBotNoFantasyAccess, I_OnBotBan, I_OnBadWordAdd,
    I_OnBadWordDel, I_OnCreateBot, I_OnDelBot, I_OnBotKick, I_OnPrePartChannel, I_OnPartChannel, I_OnLeaveChannel,
    I_OnJoinChannel, I_OnTopicUpdated, I_OnPreChanExpire, I_OnChanExpire, I_OnPreServerConnect, I_OnServerConnect,
    I_OnPreUplinkSync, I_OnServerDisconnect, I_OnRestart, I_OnShutdown, I_OnPreNickExpire, I_OnNickExpire, I_OnDefconLevel,
    I_OnExceptionAdd, I_OnExceptionDel, I_OnAddXLine, I_OnDelXLine, I_IsServicesOper, I_OnServerQuit, I_OnUserQuit,
    I_OnPreUserLogoff, I_OnPostUserLogoff, I_OnBotCreate, I_OnBotChange, I_OnBotDelete, I_OnAccessDel, I_OnAccessAdd,
    I_OnAccessClear, I_OnLevelChange, I_OnChanDrop, I_OnChanRegistered, I_OnChanSuspend, I_OnChanUnsuspend,
    I_OnCreateChan, I_OnDelChan, I_OnChannelCreate, I_OnChannelDelete, I_OnAkickAdd, I_OnAkickDel, I_OnCheckKick,
    I_OnChanInfo, I_OnCheckPriv, I_OnGroupCheckPriv, I_OnNickDrop, I_OnNickGroup, I_OnNickIdentify,
    I_OnUserLogin, I_OnNickLogout, I_OnNickRegister, I_OnNickConfirm, I_OnNickSuspend, I_OnNickUnsuspended, I_OnDelNick, I_OnNickCoreCreate,
    I_OnDelCore, I_OnChangeCoreDisplay, I_OnNickClearAccess, I_OnNickAddAccess, I_OnNickEraseAccess, I_OnNickClearCert,
    I_OnNickAddCert, I_OnNickEraseCert, I_OnNickInfo, I_OnBotInfo, I_OnCheckAuthentication, I_OnNickUpdate,
    I_OnFingerprint, I_OnUserAway, I_OnInvite, I_OnDeleteVhost, I_OnSetVhost, I_OnSetDisplayedHost, I_OnMemoSend, I_OnMemoDel,
    I_OnChannelModeSet, I_OnChannelModeUnset, I_OnUserModeSet, I_OnUserModeUnset, I_OnChannelModeAdd, I_OnUserModeAdd,
    I_OnMLock, I_OnUnMLock, I_OnModuleLoad, I_OnModuleUnload, I_OnServerSync, I_OnUplinkSync, I_OnBotPrivmsg, I_OnBotNotice,
    I_OnPrivmsg, I_OnLog, I_OnLogMessage, I_OnDnsRequest, I_OnCheckModes, I_OnChannelSync, I_OnSetCorrectModes,
    I_OnSerializeCheck, I_OnSerializableConstruct, I_OnSerializableDestruct, I_OnSerializableUpdate,
    I_OnSerializeTypeCreate, I_OnSetChannelOption, I_OnSetNickOption, I_OnMessage, I_OnCanSet, I_OnCheckDelete,
    I_OnExpireTick, I_OnNickValidate, I_OnChannelUnban,
    I_SIZE
};

/** Used to manage modules.
 */
class CoreExport ModuleManager {
  public:
    /** Event handler hooks.
     */
    static std::vector<Module *> EventHandlers[I_SIZE];

    /** List of all modules loaded in Anope
     */
    static std::list<Module *> Modules;

#ifdef _WIN32
    /** Clean up the module runtime directory
     */
    static void CleanupRuntimeDirectory();
#endif

    /** Loads a given module.
     * @param m the module to load
     * @param u the user who loaded it, NULL for auto-load
     * @return MOD_ERR_OK on success, anything else on fail
     */
    static ModuleReturn LoadModule(const Anope::string &modname, User *u);

    /** Unload the given module.
     * @param m the module to unload
     * @param u the user who unloaded it
     * @return MOD_ERR_OK on success, anything else on fail
     */
    static ModuleReturn UnloadModule(Module *m, User * u);

    /** Find a module
     * @param name The module name
     * @return The module
     */
    static Module *FindModule(const Anope::string &name);

    /** Find the first module of a certain type
     * @param type The module type
     * @return The module
     */
    static Module *FindFirstOf(ModType type);

    /** Checks whether this version of Anope is at least major.minor.patch.build
     * Throws a ModuleException if not
     * @param major The major version
     * @param minor The minor version
     * @param patch The patch version
     */
    static void RequireVersion(int major, int minor, int patch);

    /** Change the priority of one event in a module.
     * Each module event has a list of modules which are attached to that event type. If you wish to be called before or after other specific modules, you may use this
     * method (usually within void Module::Prioritize()) to set your events priority. You may use this call in other methods too, however, this is not supported behaviour
     * for a module.
     * @param mod The module to change the priority of
     * @param i The event to change the priority of
     * @param s The state you wish to use for this event. Use one of
     * PRIO_FIRST to set the event to be first called, PRIO_LAST to set it to be the last called, or PRIO_BEFORE and PRIO_AFTER
     * to set it to be before or after one or more other modules.
     * @param modules If PRIO_BEFORE or PRIO_AFTER is set in parameter 's', then this contains a list of one or more modules your module must be
     * placed before or after. Your module will be placed before the highest priority module in this list for PRIO_BEFORE, or after the lowest
     * priority module in this list for PRIO_AFTER.
     * @param sz The number of modules being passed for PRIO_BEFORE and PRIO_AFTER. Defaults to 1, as most of the time you will only want to prioritize your module
     * to be before or after one other module.
     */
    static bool SetPriority(Module *mod, Implementation i, Priority s,
                            Module **modules = NULL, size_t sz = 1);

    /** Change the priority of all events in a module.
     * @param mod The module to set the priority of
     * @param s The priority of all events in the module.
     * Note that with this method, it is not possible to effectively use PRIO_BEFORE or PRIO_AFTER, you should use the more fine tuned
     * SetPriority method for this, where you may specify other modules to be prioritized against.
     */
    static bool SetPriority(Module *mod, Priority s);

    /** Detach all events from a module (used on unload)
     * @param mod Module to detach from
     */
    static void DetachAll(Module *mod);

    /** Unloading all modules except the protocol module.
     */
    static void UnloadAll();

  private:
    /** Call the module_delete function to safely delete the module
     * @param m the module to delete
     * @return MOD_ERR_OK on success, anything else on fail
     */
    static ModuleReturn DeleteModule(Module *m);

    /** Get the version of Anope the module was compiled against
     * @return The version
     */
    static ModuleVersion GetVersion(void *handle);
};

#endif // MODULES_H
