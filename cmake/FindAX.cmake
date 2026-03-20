# ==============================================
# Try to find Cambricon MPS libraries:
# - cnrt
# - cndrv
# - cn_codec
# - cn_vo
# - cn_vps
# - cn_vgu
# - cn_g2d
# - cn_sys
#
# SET AX_INCLUDE_DIR with neuware include directory
# ==============================================

if(AXCL_HOME)
  get_filename_component(AXCL_HOME ${AXCL_HOME} ABSOLUTE)
  message(STATUS "AXCL_HOME: ${AXCL_HOME}")
elseif(DEFINED ENV{AXCL_HOME})
  get_filename_component(AXCL_HOME $ENV{AXCL_HOME} ABSOLUTE)
  message(STATUS "ENV{AXCL_HOME}: ${AXCL_HOME}")
else()
  set(AXCL_HOME "/axcl")
  message(STATUS "Default AXCL_HOME: ${AXCL_HOME}")
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
