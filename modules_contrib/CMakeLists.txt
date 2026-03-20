cmake_minimum_required(VERSION 2.8.7)
if(POLICY CMP0046)
  cmake_policy(SET CMP0046 NEW)
endif()
if(POLICY CMP0054)
  cmake_policy(SET CMP0054 NEW)
endif()

set(UNISTREAM_ROOT_DIR ${UNISTREAM_ROOT_DIR}/..)

set(LIBRARY_OUTPUT_PATH ${UNISTREAM_ROOT_DIR}/lib)

set(contrib_modules_list fakesink selector)

include_directories(${UNISTREAM_ROOT_DIR}/modules)
include_directories(${UNISTREAM_ROOT_DIR}/modules/util/include)
# ---[ unistream_va
have_modules_target(${UNISTREAM_ROOT_DIR})
# ---[ easydk
have_easydk_target(${UNISTREAM_ROOT_DIR})


foreach(contrib_module ${contrib_modules_list})
  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/${contrib_module}/include)
  install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${contrib_module}/include/ DESTINATION include)
  file(GLOB_RECURSE contrib_module_src ${CMAKE_CURRENT_SOURCE_DIR}/${contrib_module}/*.cpp)
  list(APPEND contrib_module_srcs ${contrib_module_src})
endforeach()

if(DEFINED contrib_modules_list)
  include(${UNISTREAM_ROOT_DIR}/cmake/have_unistream_target.cmake)
  have_framework_target(${UNISTREAM_ROOT_DIR})
  have_modules_target(${UNISTREAM_ROOT_DIR})
  message(STATUS "@@@@@@@@@@@ Target : unistream_contrib")
  add_library(unistream_contrib SHARED ${contrib_module_srcs})
  set_target_properties(unistream_contrib PROPERTIES LINK_FLAGS_RELEASE -s)
  if(HAVE_FRAMEWORK_TARGET)
    add_dependencies(unistream_contrib unistream_core)
  endif()
  if(HAVE_MODULES_TARGET)
    add_dependencies(unistream_contrib unistream_va)
  endif()
  target_link_libraries(unistream_contrib unistream_va)
  install(TARGETS unistream_contrib LIBRARY DESTINATION lib)
endif()
