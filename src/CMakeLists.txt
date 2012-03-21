include_directories(${CMAKE_CURRENT_SOURCE_DIR}
                    ${CMAKE_CURRENT_BINARY_DIR})

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/config.h)

file(GLOB_RECURSE vast_sources *.cc)
add_executable(vast ${vast_sources})

set(vastlibs
    ${Boost_LIBRARIES}
    ${Broccoli_LIBRARIES})
if (USE_PERFTOOLS)
    set(vastlibs ${vastlibs} ${Perftools_LIBRARIES})
endif ()

target_link_libraries(vast ${vastlibs})

install(TARGETS vast DESTINATION bin)

set(VAST_EXE vast CACHE STRING "VAST executable binary" FORCE)

#
# VAST wrapper
#

# Only inserst ${value} into the list ${var} if it is not already contained.
macro (list_unique_insert var value)
    list(FIND ${var} ${value} contained)
    if (${contained} EQUAL -1)
        LIST(APPEND ${var} ${value})
    endif ()
endmacro ()

# VAST executable (used in vast.in)
get_target_property(VAST_BINARY vast LOCATION)

# Determine correct name of environment variable that preloads shared libraries.
if (UNIX)
    if (APPLE)
        set(VAST_SYSTEM_ENV DYLD_LIBRARY_PATH)
    else ()
        set(VAST_SYSTEM_ENV LD_LIBRARY_PATH)
    endif ()
else ()
    message(FATAL_ERROR "Supporting currently only UNIX, sorry...")
endif ()

# Build the exported PATH.
get_filename_component(Broccoli_library_dirs ${Broccoli_LIBRARIES} PATH)

# A list of paths that is used to build the export string.
set(path_list)
list_unique_insert(path_list ${Broccoli_library_dirs})
foreach(i ${Boost_LIBRARIES})
    get_filename_component(dir ${i} PATH)
    if (dir)
        list_unique_insert(path_list ${dir})
    endif ()
endforeach ()

# Build the final string in the shell script.
list(LENGTH path_list path_list_length)
if (path_list_length GREATER 0)
    list(GET path_list 0 first)
    list(REMOVE_AT path_list 0)
    set(VAST_LIBRARY_PATH "${first}")
    foreach(p ${path_list})
        set(VAST_LIBRARY_PATH "${VAST_LIBRARY_PATH}:${p}")
    endforeach ()
else ()
    set(VAST_LIBRARY_PATH "")
endif()

# TODO: install only if desired.
# Copy the wrapper over to the build directory.
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/vast.in
    ${CMAKE_BINARY_DIR}/vast.sh
    @ONLY)