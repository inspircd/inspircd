/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2014 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2013, 2019 Sadie Powell <sadie@witchery.services>
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

/** Provides an easy method of reading a text file into memory. */
class CoreExport FileReader final
{
	/** The lines of text in the file. */
	std::vector<std::string> lines;

	/** File size in bytes. */
	unsigned long totalSize = 0;

 public:
	/** Initializes a new file reader. */
	FileReader() = default;

	/** Initializes a new file reader and reads the specified file.
	 * @param filename The file to read into memory.
	 */
	FileReader(const std::string& filename);

	/** Loads a text file from disk.
	 * @param filename The file to read into memory.
	 * @throw CoreException The file can not be loaded.
	 */
	void Load(const std::string& filename);

	/** Retrieves the entire contents of the file cache as a single string. */
	std::string GetString() const;

	/** Retrieves the entire contents of the file cache as a vector of strings. */
	const std::vector<std::string>& GetVector() const { return lines; }

	/** Retrieves the total size in bytes of the file. */
	unsigned long TotalSize() const { return totalSize; }
};
