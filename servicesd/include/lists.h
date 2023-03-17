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

#ifndef LISTS_H
#define LISTS_H

#include "services.h"
#include "anope.h"

/** A class to process numbered lists (passed to most DEL/LIST/VIEW commands).
 * The function HandleNumber is called for every number in the list. Note that
 * if descending is true it gets called in descending order. This is so deleting
 * the index passed to the function from an array will not cause the other indexes
 * passed to the function to be incorrect. This keeps us from having to have an
 * 'in use' flag on everything.
 */
class CoreExport NumberList {
  private:
    bool is_valid;

    std::set<unsigned> numbers;

    bool desc;
  public:
    /** Processes a numbered list
     * @param list The list
     * @param descending True to make HandleNumber get called with numbers in descending order
     */
    NumberList(const Anope::string &list, bool descending);

    /** Destructor, does nothing
     */
    virtual ~NumberList();

    /** Should be called after the constructors are done running. This calls the callbacks.
     */
    void Process();

    /** Called with a number from the list
     * @param number The number
     */
    virtual void HandleNumber(unsigned number);

    /** Called when there is an error with the numbered list
     * Return false to immediately stop processing the list and return
     * This is all done before we start calling HandleNumber, so no numbers will have been processed yet
     * @param list The list
     * @return false to stop processing
     */
    virtual bool InvalidRange(const Anope::string &list);
};

/** This class handles formatting LIST/VIEW replies.
 */
class CoreExport ListFormatter {
  public:
    typedef std::map<Anope::string, Anope::string> ListEntry;
  private:
    NickCore *nc;
    std::vector<Anope::string> columns;
    std::vector<ListEntry> entries;
  public:
    ListFormatter(NickCore *nc);
    ListFormatter &AddColumn(const Anope::string &name);
    void AddEntry(const ListEntry &entry);
    bool IsEmpty() const;
    void Process(std::vector<Anope::string> &);
};

/** This class handles formatting INFO replies
 */
class CoreExport InfoFormatter {
    NickCore *nc;
    std::vector<std::pair<Anope::string, Anope::string> > replies;
    unsigned longest;
  public:
    InfoFormatter(NickCore *nc);
    void Process(std::vector<Anope::string> &);
    Anope::string &operator[](const Anope::string &key);
    void AddOption(const Anope::string &opt);
};

#endif // LISTS_H
