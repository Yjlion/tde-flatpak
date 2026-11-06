include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}/..
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


##### kcm_tdeflatpak (module) ###################

tde_add_kpart( kcm_tdeflatpak AUTOMOC
  SOURCES kcm_tdeflatpak.cpp
  LINK tdeflatpakcore-static tdeio-shared tdeui-shared
  DESTINATION ${PLUGIN_INSTALL_DIR}
)

install( FILES tdeflatpak-settings.desktop DESTINATION ${XDG_APPS_INSTALL_DIR} )
