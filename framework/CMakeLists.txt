cmake_minimum_required(VERSION 3.5)
if(POLICY CMP0046)
  cmake_policy(SET CMP0046 NEW)
endif()
if(POLICY CMP0054)
  cmake_policy(SET CMP0054 NEW)
endif()

set(UNISTREAM_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(LIBRARY_OUTPUT_PATH ${UNISTREAM_ROOT_DIR}/lib)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin/)

set(CMAKE_MODULE_PATH ${UNISTREAM_ROOT_DIR}/cmake)

# ---[ Google-gflags
include(${UNISTREAM_ROOT_DIR}/cmake/FindGFlags.cmake)
include_directories(${GFLAGS_INCLUDE_DIRS})

# ---[ rapidjson
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../3rdparty/rapidjson/include)

set(VERSION_HEAD_FILE ${CMAKE_CURRENT_SOURCE_DIR}/include/unistream_version.hpp)
execute_process(COMMAND awk /UNISTREAM_MAJOR_VERSION/ ${VERSION_HEAD_FILE} OUTPUT_VARIABLE VERSION_STR)
string(REGEX MATCH "[0-9]+"  MAJOR_VERSION ${VERSION_STR})
execute_process(COMMAND awk /UNISTREAM_MINOR_VERSION/ ${VERSION_HEAD_FILE} OUTPUT_VARIABLE VERSION_STR)
string(REGEX MATCH "[0-9]+"  MINOR_VERSION ${VERSION_STR})
execute_process(COMMAND awk /UNISTREAM_PATCH_VERSION/ ${VERSION_HEAD_FILE} OUTPUT_VARIABLE VERSION_STR)
string(REGEX MATCH "[0-9]+"  PATCH_VERSION ${VERSION_STR})
set(UNISTREAM_TARGET_VERSION "${MAJOR_VERSION}.${MINOR_VERSION}.${PATCH_VERSION}")

# ---[ source
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src core_srcs)
aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src/profiler profiler_srcs)

if(BUILD_TESTS)
  add_definitions(-DUNIT_TEST)
endif()

message(STATUS "@@@@@@@@@@@ Target : unistream_core")
add_library(unistream_core SHARED ${core_srcs} ${profiler_srcs})
target_link_libraries(unistream_core PRIVATE ${GFLAGS_LIBRARIES} dl pthread)
set_target_properties(unistream_core PROPERTIES LINK_FLAGS_RELEASE -s)
set_target_properties(unistream_core PROPERTIES VERSION ${UNISTREAM_TARGET_VERSION})

# install
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")  #set runtime path
install(TARGETS unistream_core LIBRARY DESTINATION lib)
install(DIRECTORY include/ DESTINATION include)

if(BUILD_TESTS)
  add_subdirectory(unitest)
endif()
