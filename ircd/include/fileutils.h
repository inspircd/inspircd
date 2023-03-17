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
class CoreExport FileReader {
    /** The lines of text in the file. */
    std::vector<std::string> lines;

    /** File size in bytes. */
    unsigned long totalSize;

  public:
    /** Initializes a new file reader. */
    FileReader() : totalSize(0) { }

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
    const std::vector<std::string>& GetVector() const {
        return lines;
    }

    /** Retrieves the total size in bytes of the file. */
    unsigned long TotalSize() const {
        return totalSize;
    }
};

/** Implements methods for file system access */
class CoreExport FileSystem {
  private:
    FileSystem() { }

  public:
    /** Expands a path fragment to a full path.
     * @param base The base path to expand from
     * @param fragment The path fragment to expand on top of base.
     */
    static std::string ExpandPath(const std::string& base,
                                  const std::string& fragment);

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

    /** Gets a list of files which exist in the specified directory.
     * @param directory The directory to retrieve files from.
     * @param entries A vector which entries will be added to.
     * @param match If defined then a glob match for files to be matched against.
     * @return True if the directory could be opened; otherwise false.
     */
    static bool GetFileList(const std::string& directory,
                            std::vector<std::string>& entries, const std::string& match = "*");

    /** Determines whether the given path starts with a Windows drive letter.
     * @param path The path to validate.
     * @returns True if the path begins with a Windows drive letter; otherwise, false.
     */
    static bool StartsWithWindowsDriveLetter(const std::string& path);
};
