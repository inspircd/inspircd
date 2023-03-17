/* ChanServ core functions
 *
 * (C) 2003-2023 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 *
 * Based on the original code of Epona by Lara.
 * Based on the original code of Services by Andy Church.
 */

struct ModeLock {
    Anope::string ci;
    bool set;
    Anope::string name;
    Anope::string param;
    Anope::string setter;
    time_t created;

    virtual ~ModeLock() { }
  protected:
    ModeLock() { }
};

struct ModeLocks {
    typedef std::vector<ModeLock *> ModeList;

    virtual ~ModeLocks() { }

    /** Check if a mode is mlocked
     * @param mode The mode
     * @param An optional param
     * @param status True to check mlock on, false for mlock off
     * @return true on success, false on fail
     */
    virtual bool HasMLock(ChannelMode *mode, const Anope::string &param,
                          bool status) const = 0;

    /** Set a mlock
     * @param mode The mode
     * @param status True for mlock on, false for mlock off
     * @param param An optional param arg for + mlocked modes
     * @param setter Who is setting the mlock
     * @param created When the mlock was created
     * @return true on success, false on failure (module blocking)
     */
    virtual bool SetMLock(ChannelMode *mode, bool status,
                          const Anope::string &param = "", Anope::string setter = "",
                          time_t created = Anope::CurTime) = 0;

    /** Remove a mlock
     * @param mode The mode
     * @param status True for mlock on, false for mlock off
     * @param param The param of the mode, required if it is a list or status mode
     * @return true on success, false on failure
     */
    virtual bool RemoveMLock(ChannelMode *mode, bool status,
                             const Anope::string &param = "") = 0;

    virtual void RemoveMLock(ModeLock *mlock) = 0;

    /** Clear all mlocks on the channel
     */
    virtual void ClearMLock() = 0;

    /** Get all of the mlocks for this channel
     * @return The mlocks
     */
    virtual const ModeList &GetMLock() const = 0;

    /** Get a list of mode locks on a channel
     * @param name The mode name to get a list of
     * @return a list of mlocks for the given mode
     */
    virtual std::list<ModeLock *> GetModeLockList(const Anope::string &name) = 0;

    /** Get details for a specific mlock
     * @param mname The mode name
     * @param An optional param to match with
     * @return The MLock, if any
     */
    virtual const ModeLock *GetMLock(const Anope::string &mname,
                                     const Anope::string &param = "") = 0;

    /** Get the current mode locks as a string
     * @param complete True to show mlock parameters as well
     * @return A string of mode locks, eg: +nrt
     */
    virtual Anope::string GetMLockAsString(bool complete) const = 0;

    virtual void Check() = 0;
};
