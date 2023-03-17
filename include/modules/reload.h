/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2015 Attila Molnar <attilamolnar@hush.com>
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


#pragma once

#include "event.h"

namespace ReloadModule {
class EventListener;
class DataKeeper;

/** Container for data saved by modules before another module is reloaded.
 */
class CustomData {
    struct Data {
        EventListener* handler;
        void* data;
        Data(EventListener* Handler, void* moddata) : handler(Handler), data(moddata) { }
    };
    typedef std::vector<Data> List;
    List list;

  public:
    /** Add data to the saved state of a module.
     * The provided handler's OnReloadModuleRestore() method will be called when the reload is done with the pointer
     * provided.
     * @param handler Handler for restoring the data
     * @param data Pointer to the data, will be passed back to the provided handler's OnReloadModuleRestore() after the
     * reload finishes
     */
    void add(EventListener* handler, void* data) {
        list.push_back(Data(handler, data));
    }

    friend class DataKeeper;
};

class EventListener : public Events::ModuleEventListener {
  public:
    EventListener(Module* mod)
        : ModuleEventListener(mod, "event/reloadmodule") {
    }

    /** Called whenever a module is about to be reloaded. Use this event to save data related to the module that you want
     * to be restored after the reload.
     * @param mod Module to be reloaded
     * @param cd CustomData instance that can store your data once.
     */
    virtual void OnReloadModuleSave(Module* mod, CustomData& cd) = 0;

    /** Restore data after a reload. Only called if data was added in OnReloadModuleSave().
     * @param mod Reloaded module, if NULL the reload failed and the module no longer exists
     * @param data Pointer that was passed to CustomData::add() in OnReloadModuleSave() at the time when the module's state
     * was saved
     */
    virtual void OnReloadModuleRestore(Module* mod, void* data) = 0;
};
}
