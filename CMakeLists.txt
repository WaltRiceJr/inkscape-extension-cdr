cmake_minimum_required(VERSION 3.16)
project(cdr2svg CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_package(PkgConfig REQUIRED)
find_package(ZLIB REQUIRED)
pkg_check_modules(LCMS2 REQUIRED IMPORTED_TARGET lcms2)
pkg_check_modules(ICU REQUIRED IMPORTED_TARGET icu-i18n icu-uc)
find_package(Boost REQUIRED) # header-only usage

file(GLOB CDR_SOURCES  ${CMAKE_CURRENT_SOURCE_DIR}/src/lib/cdr/*.cpp)
file(GLOB RVNG_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/lib/rvng/*.cpp)

add_executable(cdr2svg
  src/conv/cdr2svg.cpp
  ${CDR_SOURCES}
  ${RVNG_SOURCES}
)

target_include_directories(cdr2svg PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src/inc
  ${Boost_INCLUDE_DIRS}
)

target_compile_definitions(cdr2svg PRIVATE
  BOOST_ERROR_CODE_HEADER_ONLY
  BOOST_SYSTEM_NO_DEPRECATED
)

target_link_libraries(cdr2svg PRIVATE
  ZLIB::ZLIB
  PkgConfig::LCMS2
  PkgConfig::ICU
)

install(TARGETS cdr2svg RUNTIME DESTINATION .)
