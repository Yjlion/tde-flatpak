include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_BINARY_DIR}
  ${TDE_INCLUDE_DIR}
  ${TQT_INCLUDE_DIRS}
  ${FLATPAK_INCLUDE_DIRS}
  ${GIO_INCLUDE_DIRS}
  ${JSONGLIB_INCLUDE_DIRS}
  ${ZLIB_INCLUDE_DIRS}
)

link_directories(
  ${TQT_LIBRARY_DIRS}
  ${TDE_LIB_DIR}
  ${FLATPAK_LIBRARY_DIRS}
  ${GIO_LIBRARY_DIRS}
  ${JSONGLIB_LIBRARY_DIRS}
)


##### tdeflatpakcore (static) ###################
# code shared by the manager and the Control Center module

tde_add_library( tdeflatpakcore STATIC_PIC AUTOMOC
  SOURCES
    settings.kcfgc
    flatpakbackend.cpp
    flatpakjob.cpp
    appstream.cpp
    iconcache.cpp
    flathubjson.cpp
    flathubapi.cpp
    overrides.cpp
    portalpermissions.cpp
    permissiontext.cpp
    operationdialog.cpp
    appactions.cpp
    permissionswidget.cpp
    preferenceswidget.cpp
  LINK
    tdeio-shared
    tdeui-shared
    ${FLATPAK_LIBRARIES}
    ${GIO_LIBRARIES}
    ${JSONGLIB_LIBRARIES}
    ${ZLIB_LIBRARIES}
)


##### tdeflatpak (executable) ###################

tde_add_executable( tdeflatpak AUTOMOC
  SOURCES
    main.cpp
    mainwindow.cpp
    catalog.cpp
    storepage.cpp
    installedpage.cpp
    searchpage.cpp
    updatespage.cpp
    remotespage.cpp
    addremotedialog.cpp
    appdetailsdialog.cpp
    configdialog.cpp
  LINK
    tdeflatpakcore-static
    tdeio-shared
    tdeui-shared
  DESTINATION ${BIN_INSTALL_DIR}
)


##### other data ################################

install( FILES tdeflatpak.desktop DESTINATION ${XDG_APPS_INSTALL_DIR} )
install( FILES settings.kcfg DESTINATION ${KCFG_INSTALL_DIR} RENAME tdeflatpak.kcfg )

add_subdirectory( kcm )
