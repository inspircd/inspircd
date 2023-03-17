add_library(clandestine_hash
  crc32.cpp
  digest.cpp
  keccak.cpp
  md5.cpp
  sha1.cpp
  sha256.cpp
  sha3.cpp)

install(TARGETS clandestine_hash DESTINATION lib/)
