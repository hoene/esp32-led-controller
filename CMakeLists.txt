# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(led-matrix-controller)

macro(list_subdirectories retval curdir return_relative)
  file(GLOB sub-dir html/* )
  set(list_of_dirs "")
  foreach(dir ${sub-dir})
    if(IS_DIRECTORY ${curdir}/${dir})
      if (${return_relative})
        set(list_of_dirs ${list_of_dirs} ${dir})
      else()
        set(list_of_dirs ${list_of_dirs} ${curdir}/${dir})
      endif()
    endif()
  endforeach()
  set(${retval} ${list_of_dirs})
endmacro()

# include_directories(html)
file(GLOB_RECURSE htmlfiles RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/html html/*)
message("web server files ${htmlfiles} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}")

foreach(file ${htmlfiles})
  if(${file} MATCHES ^.*\.gz$ )
    target_add_binary_data(led-matrix-controller.elf html/${file} BINARY)
    list(APPEND compressedhtmlfiles ${file})
    list(APPEND compressedhtmlpaths ${CMAKE_CURRENT_SOURCE_DIR}/html/${file})
  else()
    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/html/${file}.gz
                   COMMAND /bin/sh -c "/bin/gzip -k -c ${CMAKE_CURRENT_SOURCE_DIR}/html/${file} >${CMAKE_CURRENT_BINARY_DIR}/html/${file}.gz"
                   DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/html/${file})
    target_add_binary_data(led-matrix-controller.elf ${CMAKE_CURRENT_BINARY_DIR}/html/${file}.gz BINARY)
    list(APPEND compressedhtmlfiles ${file}.gz)
    list(APPEND compressedhtmlpaths ${CMAKE_CURRENT_BINARY_DIR}/html/${file}.gz)
  endif()   
endforeach()

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/html.c
                   COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/make-html-c.sh \'${compressedhtmlfiles}\' ${CMAKE_CURRENT_BINARY_DIR}/html.c
                   DEPENDS ${compressedhtmlpaths} ${CMAKE_CURRENT_SOURCE_DIR}/make-html-c.sh)
target_sources(led-matrix-controller.elf PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/html.c)