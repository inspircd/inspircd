# Compiling 
```
conan profile detect --force 
conan install . --output-folder=build/ --build=missing -s compiler.cppstd=11
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake
cmake --build . --parallel 8
```
