/* Mode support
 *
 * (C) 2008-2011 Adam <Adam@anope.org>
 * (C) 2008-2023 Anope Team <team@anope.org>
 *
 * Please read COPYING and README for further details.
 */

#ifndef MODES_H
#define MODES_H

#include "anope.h"
#include "base.h"

/** The different types of modes
*/
enum ModeType {
    /* Regular mode */
    MODE_REGULAR,
    /* b/e/I */
    MODE_LIST,
    /* k/l etc */
    MODE_PARAM,
    /* v/h/o/a/q */
    MODE_STATUS
};

/* Classes of modes, Channel modes and User modes
 */
enum ModeClass {
    MC_CHANNEL,
    MC_USER
};

/** This class is the basis of all modes in Anope
 */
class CoreExport Mode : public Base {
  public:
    /* Mode name */
    Anope::string name;
    /* Class of mode this is (user/channel) */
    ModeClass mclass;
    /* Mode char for this, eg 'b' */
    char mchar;
    /* Type of mode this is, eg MODE_LIST */
    ModeType type;

    /** constructor
     * @param mname The mode name
     * @param mclass The type of mode this is
     * @param mc The mode char
     * @param type The mode type
     */
    Mode(const Anope::string &mname, ModeClass mclass, char mc, ModeType type);
    virtual ~Mode();

    /** Can a user set this mode, used for mlock
     * @param u The user
     */
    virtual bool CanSet(User *u) const;
};

/** This class is a user mode, all user modes use this/inherit from this
 */
class CoreExport UserMode : public Mode {
  public:
    /** constructor
     * @param name The mode name
     * @param mc The mode char
     */
    UserMode(const Anope::string &name, char mc);
};

class CoreExport UserModeParam : public UserMode {
  public:
    /** constructor
     * @param name The mode name
     * @param mc The mode char
     */
    UserModeParam(const Anope::string &name, char mc);

    /** Check if the param is valid
     * @param value The param
     * @return true or false
     */
    virtual bool IsValid(Anope::string &value) const {
        return true;
    }
};

/** This class is a channel mode, all channel modes use this/inherit from this
 */
class CoreExport ChannelMode : public Mode {
  public:
    /* channel modes that can possibly unwrap this mode */
    std::vector<ChannelMode *> listeners;

    /** constructor
     * @param name The mode name
     * @param mc The mode char
     */
    ChannelMode(const Anope::string &name, char mc);

    bool CanSet(User *u) const anope_override;

    virtual void Check() { }

    /** 'wrap' this channel mode and param to the underlying mode and param
     */
    virtual ChannelMode *Wrap(Anope::string &param);

    /** 'unwrap' this mode to our internal representation
     */
    ChannelMode *Unwrap(Anope::string &param);

    /** called when a mode is being unwrapped, and is asking us if we can unwrap it
     */
    virtual ChannelMode *Unwrap(ChannelMode *, Anope::string &param);
};

/** This is a mode for lists, eg b/e/I. These modes should inherit from this
 */
class CoreExport ChannelModeList : public ChannelMode {
  public:
    /** constructor
     * @param name The mode name
     * @param mc The mode char
     */
    ChannelModeList(const Anope::string &name, char mc);

    /** Is the mask valid
     * @param mask The mask
     * @return true for yes, false for no
     */
    virtual bool IsValid(Anope::string &mask) const;

    /** Checks if mask affects user
     * Should only be used for extbans or other weird ircd-specific things.
     * @param u The user
     * @param e The entry to match against
     * @return true on match
     */
    virtual bool Matches(User *u, const Entry *e) {
        return false;
    }

    /** Called when a mask is added to a channel
     * @param chan The channel
     * @param mask The mask
     */
    virtual void OnAdd(Channel *chan, const Anope::string &mask) { }

    /** Called when a mask is removed from a channel
     * @param chan The channel
     * @param mask The mask
     */
    virtual void OnDel(Channel *chan, const Anope::string &mask) { }
};

/** This is a mode with a paramater, eg +k/l. These modes should use/inherit from this
*/
class CoreExport ChannelModeParam : public ChannelMode {
  public:
    /** constructor
     * @param name The mode name
     * @param mc The mode char
     * @param minus_no_arg true if this mode sends no arg when unsetting
     */
    ChannelModeParam(const Anope::string &name, char mc, bool minus_no_arg = false);

    /* Should we send an arg when unsetting this mode? */
    bool minus_no_arg;

    /** Is the param valid
     * @param value The param
     * @return true for yes, false for no
     */
    virtual bool IsValid(Anope::string &value) const {
        return true;
    }
};

/** This is a mode that is a channel status, eg +v/h/o/a/q.
*/
class CoreExport ChannelModeStatus : public ChannelMode {
  public:
    /* The symbol, eg @ % + */
    char symbol;
    /* The "level" of the mode, used to compare with other modes.
     * Used so we know op > halfop > voice etc.
     */
    unsigned level;

    /** constructor
     * @param name The mode name
     * @param mc The mode char
     * @param msymbol The symbol for the mode, eg @ %
     * @param mlevel A level for the mode, which is usually determined by the PREFIX capab
     */
    ChannelModeStatus(const Anope::string &name, char mc, char msymbol,
                      unsigned mlevel);
};

/** A virtual mode. This mode doesn't natively exist on the IRCd (like extbans),
 * but we still have a representation for it.
 */
template<typename T>
class CoreExport ChannelModeVirtual : public T {
    Anope::string base;
    ChannelMode *basech;

  public:
    ChannelModeVirtual(const Anope::string &mname, const Anope::string &basename);

    ~ChannelModeVirtual();

    void Check() anope_override;

    ChannelMode *Wrap(Anope::string &param) anope_override;

    ChannelMode *Unwrap(ChannelMode *cm, Anope::string &param) anope_override = 0;
};

/* The status a user has on a channel (+v, +h, +o) etc */
class CoreExport ChannelStatus {
    Anope::string modes;
  public:
    ChannelStatus();
    ChannelStatus(const Anope::string &modes);
    void AddMode(char c);
    void DelMode(char c);
    bool HasMode(char c) const;
    bool Empty() const;
    void Clear();
    const Anope::string &Modes() const;
    Anope::string BuildModePrefixList() const;
};

class CoreExport UserModeOperOnly : public UserMode {
  public:
    UserModeOperOnly(const Anope::string &mname, char um) : UserMode(mname, um) { }

    bool CanSet(User *u) const anope_override;
};

class CoreExport UserModeNoone : public UserMode {
  public:
    UserModeNoone(const Anope::string &mname, char um) : UserMode(mname, um) { }

    bool CanSet(User *u) const anope_override;
};

/** Channel mode +k (key)
 */
class CoreExport ChannelModeKey : public ChannelModeParam {
  public:
    ChannelModeKey(char mc) : ChannelModeParam("KEY", mc) { }

    bool IsValid(Anope::string &value) const anope_override;
};

/** This class is used for oper only channel modes
 */
class CoreExport ChannelModeOperOnly : public ChannelMode {
  public:
    ChannelModeOperOnly(const Anope::string &mname, char mc) : ChannelMode(mname,
                mc) { }

    /* Opers only */
    bool CanSet(User *u) const anope_override;
};

/** This class is used for channel modes only servers may set
 */
class CoreExport ChannelModeNoone : public ChannelMode {
  public:
    ChannelModeNoone(const Anope::string &mname, char mc) : ChannelMode(mname, mc) { }

    bool CanSet(User *u) const anope_override;
};

/** This is the mode manager
 * It contains functions for adding modes to Anope so Anope can track them
 * and do things such as MLOCK.
 * This also contains a mode stacker that will combine multiple modes and set
 * them on a channel or user at once
 */
class CoreExport ModeManager {
  public:

    /* Number of generic channel and user modes we are tracking */
    static unsigned GenericChannelModes;
    static unsigned GenericUserModes;

    /** Add a user mode to Anope
     * @param um A UserMode or UserMode derived class
     * @return true on success, false on error
     */
    static bool AddUserMode(UserMode *um);

    /** Add a channel mode to Anope
     * @param cm A ChannelMode or ChannelMode derived class
     * @return true on success, false on error
     */
    static bool AddChannelMode(ChannelMode *cm);

    /** Remove a user mode from Anope
     * @param um A UserMode to remove
     */
    static void RemoveUserMode(UserMode *um);

    /** Remove a channel mode from Anope
     * @param um A ChanneMode to remove
     */
    static void RemoveChannelMode(ChannelMode *cm);

    /** Find a channel mode
     * @param mode The mode
     * @return The mode class
     */
    static ChannelMode *FindChannelModeByChar(char mode);

    /** Find a user mode
     * @param mode The mode
     * @return The mode class
     */
    static UserMode *FindUserModeByChar(char mode);

    /** Find a channel mode
     * @param name The modename
     * @return The mode class
     */
    static ChannelMode *FindChannelModeByName(const Anope::string &name);

    /** Find a user mode
     * @param name The modename
     * @return The mode class
     */
    static UserMode *FindUserModeByName(const Anope::string &name);

    /** Gets the channel mode char for a symbol (eg + returns v)
     * @param symbol The symbol
     * @return The char
     */
    static char GetStatusChar(char symbol);

    static const std::vector<ChannelMode *> &GetChannelModes();
    static const std::vector<UserMode *> &GetUserModes();
    static const std::vector<ChannelModeStatus *> &GetStatusChannelModesByRank();
    static void RebuildStatusModes();

    /** Add a mode to the stacker to be set on a channel
     * @param bi The client to set the modes from
     * @param c The channel
     * @param cm The channel mode
     * @param set true for setting, false for removing
     * @param param The param, if there is one
     */
    static void StackerAdd(BotInfo *bi, Channel *c, ChannelMode *cm, bool set,
                           const Anope::string &param = "");

    /** Add a mode to the stacker to be set on a user
     * @param bi The client to set the modes from
     * @param u The user
     * @param um The user mode
     * @param set true for setting, false for removing
     * @param param The param, if there is one
     */
    static void StackerAdd(BotInfo *bi, User *u, UserMode *um, bool set,
                           const Anope::string &param = "");

    /** Process all of the modes in the stacker and send them to the IRCd to be set on channels/users
     */
    static void ProcessModes();

    /** Delete a user, channel, or mode from the stacker
     */
    static void StackerDel(User *u);
    static void StackerDel(Channel *c);
    static void StackerDel(Mode *m);
};

/** Represents a mask set on a channel (b/e/I)
 */
class CoreExport Entry {
    Anope::string name;
    Anope::string mask;
  public:
    unsigned short cidr_len;
    int family;
    Anope::string nick, user, host, real;

    /** Constructor
     * @param mode What mode this host is for, can be empty for unknown/no mode
     * @param host A full or partial nick!ident@host/cidr#real name mask
     */
    Entry(const Anope::string &mode, const Anope::string &host);

    /** Get the banned mask for this entry
     * @return The mask
     */
    const Anope::string GetMask() const;

    const Anope::string GetNUHMask() const;

    /** Check if this entry matches a user
     * @param u The user
     * @param full True to match against a users real host and IP
     * @return true on match
     */
    bool Matches(User *u, bool full = false) const;
};

#endif // MODES_H
