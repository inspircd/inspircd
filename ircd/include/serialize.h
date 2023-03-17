/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2019-2020 Sadie Powell <sadie@witchery.services>
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

/** Base class for serializable elements. */
class CoreExport Serializable {
  protected:
    Serializable() { }

  public:
    /** Encapsulates a chunk of serialised data. */
    class CoreExport Data {
      public:
        /** Maps keys to serialised data. */
        typedef insp::flat_map<std::string, Data> ChildMap;

        /** Maps keys to simple values. */
        typedef insp::flat_map<std::string, std::string> EntryMap;

      private:
        /** A mapping of keys to serialised data. */
        ChildMap children;

        /** A mapping of keys to values. */
        EntryMap entries;

      public:
        /** Retrieves the child elements. */
        const ChildMap& GetChildren() const {
            return children;
        }
        ChildMap& GetChildren() {
            return children;
        }

        /** Retrieves the key/value map. */
        const EntryMap& GetEntries() const {
            return entries;
        }
        EntryMap& GetEntries() {
            return entries;
        }

        /** Loads the serialised data with the specified key.
         * @param key The key by which this serialised data is identified.
         * @param out The location to store the serialised data for this key.
         */
        Data& Load(const std::string& key, Data& out);

        /** Loads the value with the specified key.
         * @param key The key by which this data is identified.
         * @param out The location to store the value for this key.
         */
        Data& Load(const std::string& key, std::string& out);

        /** Loads the value with the specified key. The value will be converted to the specified type.
         * @param key The key by which this data is identified.
         * @param out The location to store the value for this key.
         */
        template <typename T>
        Data& Load(const std::string& key, T& out) {
            // Attempt to load as a string.
            std::string str;
            Load(key, str);

            std::stringstream ss(str);
            ss >> out;
            return *this;
        }

        /** Stores the serialised data against the specified key.
         * @param key The key by which this serialised data should be stored against.
         * @param value The serialised data to store.
         */
        Data& Store(const std::string& key, const Data& value);

        /** Stores the value against the specified key.
         * @param key The key by which this value should be stored against.
         * @param value The value to store.
         */
        Data& Store(const std::string& key, const std::string& value);

        /** Stores the value against the specified key. The value will be converted to a string using ConvToStr.
         * @param key The key by which this value should be stored against.
         * @param value The value to store.
         */
        template <typename T>
        Data& Store(const std::string& key, const T& value) {
            return Store(key, ConvToStr(value));
        }
    };

    /** Deserializes the specified Data instance into this object.
     * @param data The Data object to deserialize from.
     * @return True if the deserialisation succeeded; otherwise, false.
     */
    virtual bool Deserialize(Data& data) = 0;

    /** Serializes the this object into the specified Data object.
     * @param data The Data object to serialize to.
     * @return True if the serialisation succeeded; otherwise, false.
     */
    virtual bool Serialize(Data& data) = 0;
};
