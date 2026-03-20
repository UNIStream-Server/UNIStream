cmake_minimum_required(VERSION 3.5)

# ---[ Options
option(BUILD_INSPECT "build unistream inspect" ON)


if(BUILD_INSPECT)
  add_subdirectory(inspect)
endif()

