cmake_minimum_required(VERSION 3.5)
if(POLICY CMP0046)
  cmake_policy(SET CMP0046 NEW)
endif()
if(POLICY CMP0054)
  cmake_policy(SET CMP0054 NEW)
endif()

set(UNISTREAM_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)

# compile flags
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DNDEBUG -O2")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG -g")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -D_REENTRANT -fPIC -Wno-deprecated-declarations -Wall -Werror")

set(CMAKE_MODULE_PATH ${UNISTREAM_ROOT_DIR}/cmake)

# ---[ 3rdparty
set(3RDPARTY_LIBS "")

# ---[ neuware
if (PLATFORM MATCHES "MLU370" OR PLATFORM MATCHES "MLU590")
  find_package(MLU)
  include_directories(${NEUWARE_INCLUDE_DIR})
elseif (PLATFORM MATCHES "CE3226")
  find_package(MPS)
  include_directories(${MPS_INCLUDE_DIR})
elseif (PLATFORM MATCHES "AX650N")
  find_package(AX)
  include_directories(${AXCL_INCLUDE_DIR})
else()
  message(FATAL_ERROR "Unsupported PLATFORM: ${PLATFORM}")
endif()

include(${UNISTREAM_ROOT_DIR}/cmake/have_unistream_target.cmake)

# ---[ pybind11
set(PYBIND11_PYTHON_VERSION 3)
add_subdirectory(${UNISTREAM_ROOT_DIR}/3rdparty/pybind11 ${CMAKE_CURRENT_BINARY_DIR}/pybind11)

# ---[ rapidjson
include_directories(${UNISTREAM_ROOT_DIR}/3rdparty/rapidjson/include)

# ---[ libyuv
include_directories(${UNISTREAM_ROOT_DIR}/3rdparty/libyuv/include)
link_directories(${UNISTREAM_ROOT_DIR}/3rdparty/libyuv/lib)
list(APPEND 3RDPARTY_LIBS ${UNISTREAM_ROOT_DIR}/3rdparty/libyuv/lib/libyuv.a)

# ---[ opencv
set(OpenCV_FIND_QUIETLY true)
#find_package(OpenCV REQUIRED) //this will include opencv_ts which result in crash on centos
find_package(OpenCV OPTIONAL_COMPONENTS videoio)
set(optional_opencv_libs ${OpenCV_LIBS})
find_package(OpenCV REQUIRED core imgproc highgui features2d)
if(OpenCV_FOUND)
  if(imgcodecs_FOUND)
    list(APPEND OpenCV_LIBS optional_opencv_libs)
  endif()
  include_directories(${OpenCV_INCLUDE_DIRS})
  list(APPEND 3RDPARTY_LIBS ${OpenCV_LIBS})
  message(STATUS "opencv include: ${OpenCV_INCLUDE_DIRS}")
  message(STATUS "opencv libraries: ${OpenCV_LIBS}")
else()
  message(FATAL_ERROR "opencv not found!")
endif()

# ---[ ffmpeg
if(WITH_FFMPEG AND (BUILD_SOURCE OR BUILD_VENCODE))
  find_package(FFmpeg REQUIRED)
  if (FFMPEG_FOUND)
    include_directories(${FFMPEG_INCLUDE_DIR})
    list(APPEND 3RDPARTY_LIBS ${FFMPEG_LIBRARIES})
    set(HAVE_FFMPEG true)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DHAVE_FFMPEG")
    if (WITH_FFMPEG_AVDEVICE)
       add_definitions(-DHAVE_FFMPEG_AVDEVICE)
    endif ()
    message(STATUS "ffmpeg include: ${FFMPEG_INCLUDE_DIR}")
    message(STATUS "ffmpeg libraries: ${FFMPEG_LIBRARIES}")
  else ()
    message(FATAL_ERROR "ffmpeg not found!")
  endif ()
endif()

# ---[ freetype
if(WITH_FREETYPE AND BUILD_OSD)
  if (PLATFORM MATCHES "CE3226")
    include_directories(${CMAKE_FIND_ROOT_PATH}/include/freetype2)
  endif()
  find_package(Freetype REQUIRED)
  if (FREETYPE_FOUND)
    include_directories(${FREETYPE_INCLUDE_DIR_ft2build})
    list(APPEND 3RDPARTY_LIBS ${FREETYPE_LIBRARIES})
    message(STATUS "freetype include: ${FREETYPE_INCLUDE_DIR_ft2build}")
    message(STATUS "freetype libraries: ${FREETYPE_LIBRARIES}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DHAVE_FREETYPE")
  else (FREETYPE_FOUND)
    message(FATAL_ERROR "freetype not found!")
  endif (FREETYPE_FOUND)
endif()

# ---[ live555
if(BUILD_SOURCE OR BUILD_VENCODE)
  list(APPEND ADDITIONAL_MAKE_CLEAN_FILES_LIST "${UNISTREAM_ROOT_DIR}/3rdparty/live555")
  list(APPEND DEPENDENCIES live555)

  include_directories(${UNISTREAM_ROOT_DIR}/3rdparty/live555/include/BasicUsageEnvironment)
  include_directories(${UNISTREAM_ROOT_DIR}/3rdparty/live555/include/groupsock)
  include_directories(${UNISTREAM_ROOT_DIR}/3rdparty/live555/include/liveMedia)
  include_directories(${UNISTREAM_ROOT_DIR}/3rdparty/live555/include/UsageEnvironment)
  set(Live555_LIBS ${UNISTREAM_ROOT_DIR}/3rdparty/live555/lib/libliveMedia.so
	           ${UNISTREAM_ROOT_DIR}/3rdparty/live555/lib/libUsageEnvironment.so
		   ${UNISTREAM_ROOT_DIR}/3rdparty/live555/lib/libBasicUsageEnvironment.so
		   ${UNISTREAM_ROOT_DIR}/3rdparty/live555/lib/libgroupsock.so)
  list(APPEND 3RDPARTY_LIBS ${Live555_LIBS})
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DHAVE_LIVE555 -DNO_OPENSSL")
endif()

# ---[ kafka
if(BUILD_KAFKA)
  find_package(LibRDKafka)
  if(LIBRDKAFKA_FOUND)
    include_directories(${LibRDKafka_INCLUDE_DIR})
    list(APPEND 3RDPARTY_LIBS ${LibRDKafka_C_LIBRARIES})
    message(STATUS "LibRDKafka include: ${LibRDKafka_INCLUDE_DIR}")
    message(STATUS "LibRDKafka libraries: ${LibRDKafka_C_LIBRARIES}")
  else()
    message(FATAL_ERROR "rdkafka not found!")
  endif()
endif()

set(LIBRARY_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/lib/)

include_directories(${UNISTREAM_ROOT_DIR}/modules)
include_directories(${UNISTREAM_ROOT_DIR}/framework/core/include)
include_directories(${UNISTREAM_ROOT_DIR}/samples/common)
include_directories(${UNISTREAM_ROOT_DIR}/modules/core/include)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

list(APPEND module_list source inference util)
foreach(module ${module_list})
  include_directories(${UNISTREAM_ROOT_DIR}/modules/${module}/include)
endforeach()

aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/src/ unistream_pyapi_srcs)
aux_source_directory(${UNISTREAM_ROOT_DIR}/samples/common/ sample_common_srcs)
aux_source_directory(${UNISTREAM_ROOT_DIR}/samples/common/preprocess postprocess_srcs)
aux_source_directory(${UNISTREAM_ROOT_DIR}/samples/common/postprocess preprocess_srcs)

include_directories(${UNISTREAM_ROOT_DIR}/3rdparty/pybind11/include)
include(${UNISTREAM_ROOT_DIR}/cmake/have_unistream_target.cmake)
have_framework_target(${UNISTREAM_ROOT_DIR})
have_easydk_target(${UNISTREAM_ROOT_DIR})
have_modules_target(${UNISTREAM_ROOT_DIR})
have_modules_contrib_target(${UNISTREAM_ROOT_DIR})

pybind11_add_module(unistream SHARED ${unistream_pyapi_srcs} ${preprocess_srcs} ${postprocess_srcs} ${sample_common_srcs})
find_package(PythonLibs REQUIRED)
include_directories(${PYTHON_INCLUDE_DIR})
# message(${PYTHON_INCLUDE_DIR})
# message(${PYTHON_LIBRARY})

if(HAVE_FRAMEWORK_TARGET)
  add_dependencies(unistream unistream_core)
endif()
if(HAVE_EASYDK_TARGET)
  add_dependencies(unistream easydk)
endif()
if(HAVE_MODULES_TARGET)
  add_dependencies(unistream unistream_va)
endif()
if(HAVE_MODULES_CONTRIB_TARGET)
  add_dependencies(unistream unistream_contrib)
endif()

target_link_libraries(unistream PRIVATE unistream_va unistream_core ${3RDPARTY_LIBS})

if(HAVE_MODULES_CONTRIB)
  set(CMAKE_EXE_LINKER_FLAGS "-Wl,--no-as-needed")
  target_link_libraries(unistream PRIVATE unistream_contrib)
endif()

if(BUILD_TESTS)
  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)
  aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR}/test unistream_pyapi_cpptest_srcs)
  pybind11_add_module(unistream_cpptest MODULE ${unistream_pyapi_cpptest_srcs})
  add_dependencies(unistream_cpptest unistream)
  target_link_libraries(unistream_cpptest PRIVATE unistream)
endif()

