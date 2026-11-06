#################################################
#
#  tdeflatpak - Flatpak manager for the Trinity Desktop Environment
#
#################################################

cmake_minimum_required( VERSION 3.5 )

project( tdeflatpak )

set( PACKAGE tdeflatpak )
set( VERSION 0.1.0 )


##### include our cmake modules #################

include( TDEMacros )
include( TDESetupPaths )
tde_setup_paths( )


##### setup architecture flags ##################

tde_setup_architecture_flags( )

include( TestBigEndian )
test_big_endian( WORDS_BIGENDIAN )

tde_setup_largefiles( )


##### optional stuff ############################

option( WITH_GCC_VISIBILITY "Enable fvisibility and fvisibility-inlines-hidden" OFF )


##### dependencies ##############################

include( FindPkgConfig )

find_package( TQt )
find_package( TDE )

pkg_check_modules( FLATPAK REQUIRED flatpak>=1.6 )
pkg_check_modules( GIO REQUIRED gio-2.0 )

find_package( ZLIB REQUIRED )


##### global compiler settings ##################

add_definitions( -DHAVE_CONFIG_H )

set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${TQT_CXX_FLAGS}" )
set( CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-undefined" )
set( CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} -Wl,--no-undefined" )

tde_setup_gcc_visibility( )


##### config.h ##################################

configure_file( config.h.cmake config.h @ONLY )


##### source directories ########################

add_subdirectory( src )
add_subdirectory( icons )
add_subdirectory( doc )
