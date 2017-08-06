Building InspIRCd for Windows:

Prerequisites:
	Visual Studio 2015 or newer (https://www.visualstudio.com/)
	CMake 2.8 or newer (http://www.cmake.org/)
	If building the installer, NSIS http://nsis.sourceforge.net/

Configuring:
	First copy any extra modules from extras (such as m_mysql) to the modules directory that you want to build.

	Run CMake to generate build files. This can be done using the CMake GUI by setting the source code path to "win",
	and the binary path to "win/build", followed by pressing "Configure". Modify any variables you need, such as install
	prefix, and then press "Generate".
	
	Alternatively CMake can be run from Command Prompt from the "win\build" directory, eg:
	
	c:\Users\Adam\Desktop\inspircd\win\build>cmake -G "Visual Studio 11" ..
	-- Check for working CXX compiler using: Visual Studio 11
	-- Check for working CXX compiler using: Visual Studio 11 -- works
	-- Detecting CXX compiler ABI info
	-- Detecting CXX compiler ABI info - done
	-- Configuring done
	-- Generating done
	-- Build files have been written to: C:/Users/Adam/Desktop/inspircd/win/build

	This generates project files for Visual Studio 11 (2012). Available generators can be seen in cmake --help,
	such as Visual Studio 10 and NMake Makefiles.
	
	If some of the modules you are building require libraries that are not in the default system path
	(and thus not found by CMake), you can inform CMake about them by defining EXTRA_INCLUDES and
	EXTRA_LIBS when configuring, eg;
	
	cmake -DEXTRA_INCLUDES:STRING="C:\inspircd-includes" -DEXTRA_LIBS:STRING="C:\inspircd-libs" -G "Visual Studio 11" ..
	
	See the CMake GUI for a full list of variables that can be set.
	
	Additionally, place any DLL files required by any extra modules in to the win directory for the installer to pick up.

Building:
	Open the InspIRCd Microsoft Visual Studio Solution file. If you are building a release, be sure to change
	the Solution Configuration to Release before starting the build. Start the build by right clicking the
	InspIRCd solution in the solution explorer and clicking "Build Solution"
	
	If you are building using NMake Makefiles, simply use "nmake".

Installing:
	If you do not want to build the installer you can simply build the INSTALL target, which will probably install
	InspIRCd into C:\Program Files\InspIRCd. This may require administrative privileges by Visual Studio.
	
	If you are building using NMake Makefiles, simply use "nmake install".
	
Building the installer:
	Locate the PACKAGE project on Visual Studio's Solution Explorer and build it. This will generate an InspIRCd-x.x.x.exe
	installer in the build directory which is ready to be distributed.
	
	If you are building using NMake Makefiles or do not want to build the installer in Visual Studio, simply use "cpack".