/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2013, 2019, 2021-2022 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012, 2014-2015 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2009 Daniel De Graaf <danieldg@inspircd.org>
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

class ExtensionItem;

/** Types of extensible that an extension can extend. */
enum class ExtensionType
	: uint8_t
{
	/** The extension extends the User class. */
	USER = 0,

	/** The extension extends the Channel class. */
	CHANNEL = 1,

	/** The extension extends the Membership class. */
	MEMBERSHIP = 2,
};

/** Base class for types which can be extended with additional data. */
class CoreExport Extensible
	: public Cullable
{
public:
	/** The container which extension values are stored in. */
	typedef insp::flat_map<ExtensionItem*, void*> ExtensibleStore;

	/** Allows extensions to access the extension store. */
	friend class ExtensionItem;

	/** The type of extensible that this is. */
	const ExtensionType extype:2;

	~Extensible() override;

	/** @copydoc Cullable::Cull */
	Cullable::Result Cull() override;

	/** Frees all extensions attached to this extensible. */
	void FreeAllExtItems();

	/** Retrieves the values for extensions which are set on this extensible. */
	const ExtensibleStore& GetExtList() const { return extensions; }

	/** Unhooks the specifies extensions from this extensible.
	 * @param items The items to unhook.
	 */
	void UnhookExtensions(const std::vector<ExtensionItem*>& items);

protected:
	Extensible(ExtensionType exttype);

private:
	/** The values for extensions which are set on this extensible. */
	ExtensibleStore extensions;

	/** Whether this extensible has been culled yet. */
	bool culled:1;
};

/** Manager for the extension system */
class CoreExport ExtensionManager final
{
public:
	/** The container which registered extensions are stored in. */
	typedef std::map<std::string, ExtensionItem*, irc::insensitive_swo> ExtMap;

	/** Begins unregistering extensions belonging to the specified module.
	 * @param module The module to unregister extensions for.
	 * @param list The list to add unregistered extensions to.
	 */
	void BeginUnregister(Module* module, std::vector<ExtensionItem*>& list);

	/** Retrieves registered extensions keyed by their names. */
	const ExtMap& GetExts() const { return types; }

	/** Retrieves an extension by name.
	 * @param name The name of the extension to retrieve.
	 * @return Either the value of this extension or nullptr if it does not exist.
	 */
	ExtensionItem* GetItem(const std::string& name);

	/** Registers an extension with the manager.
	 * @return Either true if the extension was registered or false if an extension with the same
	 *         name already exists.
	 */
	bool Register(ExtensionItem* item);

private:
	/** Registered extensions keyed by their names. */
	ExtMap types;
};
