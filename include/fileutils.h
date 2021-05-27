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

/** Represents the position within a file. */
class CoreExport FilePosition
{
 public:
	/** The name of the file that the position points to. */
	std::string name;

	/** The line of the file that this position points to. */
	unsigned long line;

	/** The column of the file that this position points to. */
	unsigned long column;

	/** Initialises a new file position with the specified name, line, and column.
	 * @param Name The name of the file that the position points to.
	 * @param Line The line of the file that this position points to.
	 * @param Column The column of the file that this position points to.
	 */
	FilePosition(const std::string& Name, unsigned long Line, unsigned long Column);

	/** Returns a string that represents this file position. */
	std::string str() const;
};

/** Provides an easy method of reading a text file into memory. */
class CoreExport FileReader
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

/** Implements methods for file system access */
class CoreExport FileSystem
{
private:
	FileSystem() = delete;

public:
	/** Expands a path fragment to a full path.
	 * @param base The base path to expand from
	 * @param fragment The path fragment to expand on top of base.
	 */
	static std::string ExpandPath(const std::string& base, const std::string& fragment);

	/**
	 * Checks whether a file with the specified name exists on the filesystem.
	 * @param path The path to a file.
	 * @return True if the file exists; otherwise, false.
	*/
	static bool FileExists(const std::string& path);

	/** Gets the file name segment of a path.
	 * @param path The path to extract the file name from.
	 * @return The file name segment of a path.
	 */
	static std::string GetFileName(const std::string& path);

	/** Determines whether the given path starts with a Windows drive letter.
	 * @param path The path to validate.
	 * @returns True if the path begins with a Windows drive letter; otherwise, false.
	 */
	static bool StartsWithWindowsDriveLetter(const std::string& path);
};
