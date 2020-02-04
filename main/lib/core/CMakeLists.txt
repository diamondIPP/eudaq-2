# GET_FILENAME_COMPONENT(OBJNAME ${CMAKE_CURRENT_LIST_DIR} NAME)

list(APPEND CMAKE_PREFIX_PATH $ENV{ROOTSYS})
find_package(ROOT QUIET)
cmake_dependent_option(StandardWaveform "StandardWaveform (requires ROOT)" OFF "ROOT_FOUND" OFF)

IF(StandardWaveform)
  include_directories(${ROOT_INCLUDE_DIRS})
  if(ROOT_USE_FILE)
    include(${ROOT_USE_FILE})
  endif()
  if(ROOT_VERSION_MAJOR GREATER 5)
    add_definitions(-DEUDAQ_LIB_ROOT6)
  endif()
ENDIF()

add_definitions("-DEUDAQ_CORE_EXPORTS")

set(EUDAQ_INCLUDE_DIRS
  ${CMAKE_CURRENT_LIST_DIR}/include
  ${CMAKE_CURRENT_LIST_DIR}/include/eudaq
  CACHE INTERNAL "directory of eudaq include")

include_directories(include)
include_directories(include/eudaq)

aux_source_directory(src CORE_SRC)
add_library(${EUDAQ_CORE_LIBRARY} SHARED ${CORE_SRC})
set_target_properties(${EUDAQ_CORE_LIBRARY} PROPERTIES DEFINE_SYMBOL "core_EXPORTS" )
set_target_properties(${EUDAQ_CORE_LIBRARY} PROPERTIES VERSION ${EUDAQ_VERSION_MAJOR}.${EUDAQ_VERSION_MINOR})

if(CMAKE_COMPILER_IS_GNUCXX AND (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 5.3))
  list(APPEND ADDITIONAL_LIBRARIES stdc++fs)
endif()

list(APPEND ADDITIONAL_LIBRARIES ${CMAKE_DL_LIBS})
target_link_libraries(${EUDAQ_CORE_LIBRARY} ${EUDAQ_THREADS_LIB} ${ADDITIONAL_LIBRARIES} ${ROOT_LIBRARIES})
target_include_directories(${EUDAQ_CORE_LIBRARY} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}> $<INSTALL_INTERFACE:include>)

install(TARGETS ${EUDAQ_CORE_LIBRARY}
  EXPORT eudaqTargets
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  # PUBLIC_HEADER DESTINATION include/eudaq
  )


file(GLOB INC_FILES "${CMAKE_CURRENT_SOURCE_DIR}/include/eudaq/*.hh")
install(FILES ${INC_FILES} DESTINATION include/eudaq)
