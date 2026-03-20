cmake_minimum_required(VERSION 3.5)

# ---[ easydk project
project(easydk C CXX)

set(EDK_VERSION_MAJOR 1)
set(EDK_VERSION_MINOR 0)
set(EDK_VERSION_PATCH 0)
set(EDK_VERSION "${EDK_VERSION_MAJOR}.${EDK_VERSION_MINOR}.${EDK_VERSION_PATCH}")

#######################################################################
if (NOT PLATFORM)
  set(PLATFORM "AX650N" CACHE STRING "Build target platform")
endif()

option(BUILD_PYTHON_API "Build Python API" OFF)
option(BUILD_TESTS "Build all of modules' unit-tests" ON)
option(BUILD_SAMPLES "Build samples" ON)
option(CODE_COVERAGE_TEST "Build code coverage test" OFF)
option(UNIIS_WITH_CURL "Build infer server with curl" ON)
option(UNIIS_RECORD_PERF "Enable record performance" ON)

option(SANITIZE_MEMORY "Enable MemorySanitizer for sanitized targets." OFF)
option(SANITIZE_ADDRESS "Enable AddressSanitizer for sanitized targets." OFF)
option(SANITIZE_THREAD "Enable ThreadSanitizer for sanitized targets." OFF)
option(SANITIZE_UNDEFINED "Enable UndefinedBehaviorSanitizer for sanitized targets." OFF)

#######################################################################
set(CMAKE_SYSTEM_NAME Linux)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Werror -fPIC -D_REENTRANT")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11 -Wall -fPIC -D_REENTRANT")

set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-DNDEBUG -O1")
set(CMAKE_SHARED_LINKER_FLAGS "-Wl,--no-as-needed")

if(CODE_COVERAGE_TEST)
  message(STATUS "Build with code coverage test")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
endif()

message(STATUS "CMAKE_INSTALL_PREFIX = ${CMAKE_INSTALL_PREFIX}")
message(STATUS "CMAKE_CURRENT_SOURCE_DIR = ${CMAKE_CURRENT_SOURCE_DIR}")

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake ${CMAKE_CURRENT_SOURCE_DIR}/cmake/sanitizers)
find_package(Sanitizers)

if (PLATFORM MATCHES "AX650N")
  add_definitions(-DPLATFORM_AX650N)
  find_package(AX)
else()
  message(FATAL_ERROR "Unsupported PLATFORM: ${PLATFORM}")
endif()

if(HAVE_CN_VB_HEADER)
  add_definitions(-DMPS_VERSION_MAJOR=1)
  add_definitions(-DMPS_VERSION_MINOR=1)
  add_definitions(-DMPS_VERSION_MICRO=0)
else()
  add_definitions(-DMPS_VERSION_MAJOR=1)
  add_definitions(-DMPS_VERSION_MINOR=0)
  add_definitions(-DMPS_VERSION_MICRO=0)
endif()

if(HAVE_MM_COMMON_HEADER)
  add_definitions(-DHAVE_MM_COMMON_HEADER)
endif()

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
message(STATUS "!!!!! BUILD ${CMAKE_BUILD_TYPE} !!!!!")
message(STATUS "!!!!! PLATFORM ${PLATFORM} !!!!")

#######################################################################

# set runtime path
set(LIBRARY_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/lib)

if (PLATFORM MATCHES "AX650N")
  set(CMAKE_BUILD_RPATH "${CMAKE_INSTALL_RPATH}/lib")
  set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
else()
  message(FATAL_ERROR "Unsupported PLATFORM: ${PLATFORM}")
endif()

# ------------------- Sources -----------------------

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/mps_config.h.in ${CMAKE_CURRENT_SOURCE_DIR}/include/mps_config.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/edk_config.h.in ${CMAKE_CURRENT_SOURCE_DIR}/include/edk_config.h)
install(FILES edk_config.h DESTINATION include)
install(DIRECTORY include DESTINATION include)
install(DIRECTORY include/infer_server/uniis DESTINATION include)

file(GLOB edk_src ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
file(GLOB common_src ${CMAKE_CURRENT_SOURCE_DIR}/src/common/*.cpp)
file(GLOB_RECURSE infer_server_srcs ${CMAKE_CURRENT_SOURCE_DIR}/src/infer_server/*.cpp)
file(GLOB_RECURSE transform_srcs ${CMAKE_CURRENT_SOURCE_DIR}/src/transform/*.cpp)
if (PLATFORM MATCHES "AX650N")
  file(GLOB_RECURSE platform_srcs ${CMAKE_CURRENT_SOURCE_DIR}/src/ax650/*.cpp)
endif()
message(STATUS "platform_srcs = ${platform_srcs}")


list(APPEND srcs ${infer_server_srcs} ${platform_srcs} ${transform_srcs} ${common_src} ${edk_src})

message(STATUS "@@@@@@@@@@@ Target : easydk")

add_library (easydk SHARED ${srcs})
target_include_directories(easydk PRIVATE
                           ${CMAKE_CURRENT_SOURCE_DIR}/include
                           ${CMAKE_CURRENT_SOURCE_DIR}/include/infer_server
                           ${CMAKE_CURRENT_SOURCE_DIR}/src/infer_server
                           ${CMAKE_CURRENT_SOURCE_DIR}/src/transform/)

if (PLATFORM MATCHES "AX650N")
  message(STATUS "axcl_rt_LIBRARY = ${axcl_rt_LIBRARY}")
  target_include_directories(easydk PRIVATE
                             ${AXCL_INCLUDE_DIR})
  target_link_libraries(easydk PRIVATE ${axcl_rt_LIBRARY} ${axcl_sys_LIBRARY} ${axcl_vdec_LIBRARY} ${axcl_dmadim_LIBRARY} ${axcl_ivps_LIBRARY} ${axcl_venc_LIBRARY})
  message(STATUS ${axcl_sys_LIBRARY})
endif()

# ---[ glog
find_package(Glog REQUIRED)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/FindGlog.cmake)
include_directories(${GLOG_INCLUDE_DIRS})

target_link_libraries(easydk PRIVATE ${GLOG_LIBRARIES} -dl -pthread -lm)

set_target_properties(easydk PROPERTIES VERSION ${EDK_VERSION})
set_target_properties(easydk PROPERTIES SOVERSION ${EDK_VERSION_MAJOR})
install(TARGETS easydk LIBRARY DESTINATION lib64)


if (UNIIS_WITH_CURL)
  find_package(CURL REQUIRED)
  add_definitions(-DUNIIS_HAVE_CURL)
  target_include_directories(easydk PRIVATE ${CURL_INCLUDE_DIR})
  target_link_libraries(easydk PUBLIC ${CURL_LIBRARIES})
endif()

if (UNIIS_RECORD_PERF)
  add_definitions(-DUNIIS_RECORD_PERF)
endif()

if (BUILD_PYTHON_API)
  add_subdirectory(python)
endif()

if (${CMAKE_PROJECT_NAME} MATCHES "easydk")
  if (BUILD_SAMPLES)
    message(STATUS "---------------- Build samples -----------------")
    add_subdirectory(3rdparty/libyuv)
    add_subdirectory(samples)
  endif()

  if (BUILD_TESTS)
    message(STATUS "----------------- Build tests ------------------")
    set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin/)
    add_subdirectory(3rdparty/googletest)
    add_subdirectory(unitest)
  endif()

endif()
