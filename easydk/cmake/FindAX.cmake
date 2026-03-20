# ==============================================
# Try to find AX650 libraries:
# - axcl_sys
# - axcl_vdec
# - axcl_rt
# - axcl_dmadim
# - axcl_ivps
# - axcl_venc
#
# SET MPS_INCLUDE_DIR with neuware include directory
# ==============================================

if(MPS_HOME)
  get_filename_component(MPS_HOME ${MPS_HOME} ABSOLUTE)
  message(STATUS "MPS_HOME: ${MPS_HOME}")
elseif(DEFINED ENV{MPS_HOME})
  get_filename_component(MPS_HOME $ENV{MPS_HOME} ABSOLUTE)
  message(STATUS "ENV{MPS_HOME}: ${MPS_HOME}")
else()
  set(MPS_HOME "/mps")
  message(STATUS "Default MPS_HOME: ${MPS_HOME}")
endif()

set(AXCL_INCLUDE_DIR  "/usr/include/axcl")
set(AXCL_LIB_DIR  "/usr/lib/axcl")

if((NOT EXISTS ${AXCL_INCLUDE_DIR}) OR (NOT EXISTS ${AXCL_LIB_DIR}))
  message(FATAL_ERROR "AXCL_SDK: ${AXCL_INCLUDE_DIR} or ${AXCL_LIB_DIR} not exists!")
endif()

set(AXCL_LIBRARIES axcl_sys axcl_vdec axcl_rt axcl_dmadim axcl_ivps axcl_venc)

# 循环查找所有库
foreach(lib ${AXCL_LIBRARIES})
    find_library(${lib}_LIBRARY
                 NAMES ${lib}
                 PATHS ${AXCL_LIB_DIR}
                 NO_DEFAULT_PATH)
    
    # 检查是否找到库
    if(NOT ${lib}_LIBRARY)
        message(FATAL_ERROR "Library ${lib} not found!")
    endif()
endforeach()
