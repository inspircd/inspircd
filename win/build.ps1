cd $PSScriptRoot/build
conan install --build=missing ..
cmake .. -A x64 -D CMAKE_BUILD_TYPE=Release
msbuild PACKAGE.vcxproj /P:Configuration=Release /P:Platform=x64 /T:PACKAGE /VERBOSITY:MINIMAL
