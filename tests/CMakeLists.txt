include_directories(
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_BINARY_DIR}/src
  ${CMAKE_BINARY_DIR}
  ${TDE_INCLUDE_DIR}
  ${TQT_INCLUDE_DIRS}
)

link_directories(
  ${TQT_LIBRARY_DIRS}
  ${TDE_LIB_DIR}
  ${FLATPAK_LIBRARY_DIRS}
  ${GIO_LIBRARY_DIRS}
  ${JSONGLIB_LIBRARY_DIRS}
)

add_definitions( -DFIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures" )

tde_add_executable( tdeflatpak-unittests
  SOURCES unittests.cpp
  LINK tdeflatpakcore-static
)

add_test( NAME unittests COMMAND tdeflatpak-unittests )
