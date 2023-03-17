/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019 Sadie Powell <sadie@witchery.services>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


/// $ModAuthor: Sadie Powell
/// $ModAuthorMail: sadie@witchery.services
/// $ModDesc: Sets channel mode +s (secret) when users try to set channel mode +p (private).
/// $ModDepends: core 3

#include "inspircd.h"

class NoPrivateMode : public Module {
  private:
    ChanModeReference privatemode;
    ChanModeReference secretmode;

  public:
    NoPrivateMode()
        : privatemode(this, "private")
        , secretmode(this, "secret") {
    }

    ModResult OnPreMode(User* source, User*, Channel* channel,
                        Modes::ChangeList& modes) CXX11_OVERRIDE {
        // We only care about channel mode changes from local users
        if (!IS_LOCAL(source) || !channel) {
            return MOD_RES_PASSTHRU;
        }

        Modes::ChangeList::List& list = modes.getlist();
        for (Modes::ChangeList::List::iterator iter = list.begin(); iter != list.end(); ++iter) {
            if (iter->mh == *privatemode) {
                iter->mh = *secretmode;
            }
        }
        return MOD_RES_PASSTHRU;
    }

    Version GetVersion() CXX11_OVERRIDE {
        return Version("Sets channel mode +s (secret) when users try to set channel mode +p (private).", VF_OPTCOMMON);
    }
};

MODULE_INIT(NoPrivateMode)

