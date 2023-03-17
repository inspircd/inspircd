/* Modular support
 *
 * (C) 2003-2023 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 */

#include "services.h"
#include "modules.h"
#include "language.h"
#include "account.h"

#ifdef GETTEXT_FOUND
# include <libintl.h>
#endif

Module::Module(const Anope::string &modname, const Anope::string &,
               ModType modtype) : name(modname), type(modtype) {
    this->handle = NULL;
    this->permanent = false;
    this->created = Anope::CurTime;
    this->SetVersion(Anope::Version());

    if (type & VENDOR) {
        this->SetAuthor("Anope");
    } else {
        /* Not vendor implies third */
        type |= THIRD;
        this->SetAuthor("Unknown");
    }

    if (ModuleManager::FindModule(this->name)) {
        throw CoreException("Module already exists!");
    }

    if (Anope::NoThird && type & THIRD) {
        throw ModuleException("Third party modules may not be loaded");
    }

    ModuleManager::Modules.push_back(this);

#if GETTEXT_FOUND
    for (unsigned i = 0; i < Language::Languages.size(); ++i) {
        /* Remove .UTF-8 or any other suffix */
        Anope::string lang;
        sepstream(Language::Languages[i], '.').GetToken(lang);

        if (Anope::IsFile(Anope::LocaleDir + "/" + lang + "/LC_MESSAGES/" + modname +
                          ".mo")) {
            if (!bindtextdomain(this->name.c_str(), Anope::LocaleDir.c_str())) {
                Log() << "Error calling bindtextdomain, " << Anope::LastError();
            } else {
                Log() << "Found language file " << lang << " for " << modname;
                Language::Domains.push_back(modname);
            }
            break;
        }
    }
#endif
}

Module::~Module() {
    UnsetExtensibles();

    /* Detach all event hooks for this module */
    ModuleManager::DetachAll(this);
    IdentifyRequest::ModuleUnload(this);
    /* Clear any active timers this module has */
    TimerManager::DeleteTimersFor(this);

    std::list<Module *>::iterator it = std::find(ModuleManager::Modules.begin(),
                                       ModuleManager::Modules.end(), this);
    if (it != ModuleManager::Modules.end()) {
        ModuleManager::Modules.erase(it);
    }

#if GETTEXT_FOUND
    std::vector<Anope::string>::iterator dit = std::find(Language::Domains.begin(),
            Language::Domains.end(), this->name);
    if (dit != Language::Domains.end()) {
        Language::Domains.erase(dit);
    }
#endif
}

void Module::SetPermanent(bool state) {
    this->permanent = state;
}

bool Module::GetPermanent() const {
    return this->permanent;
}

void Module::SetVersion(const Anope::string &nversion) {
    this->version = nversion;
}

void Module::SetAuthor(const Anope::string &nauthor) {
    this->author = nauthor;
}

void Module::Prioritize() {
}

ModuleVersion::ModuleVersion(const ModuleVersionC &ver) {
    version_major = ver.version_major;
    version_minor = ver.version_minor;
    version_patch = ver.version_patch;
}

int ModuleVersion::GetMajor() const {
    return this->version_major;
}

int ModuleVersion::GetMinor() const {
    return this->version_minor;
}

int ModuleVersion::GetPatch() const {
    return this->version_patch;
}
