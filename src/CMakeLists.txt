include_directories(
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_BINARY_DIR}
  ${TDE_INCLUDE_DIR}
  ${TQT_INCLUDE_DIRS}
  ${FLATPAK_INCLUDE_DIRS}
  ${GIO_INCLUDE_DIRS}
  ${ZLIB_INCLUDE_DIRS}
)

link_directories(
  ${TQT_LIBRARY_DIRS}
  ${TDE_LIB_DIR}
  ${FLATPAK_LIBRARY_DIRS}
  ${GIO_LIBRARY_DIRS}
)


##### tdeflatpak (executable) ###################

tde_add_executable( tdeflatpak AUTOMOC
  SOURCES
    main.cpp
    mainwindow.cpp
    installedpage.cpp
    searchpage.cpp
    updatespage.cpp
    remotespage.cpp
    addremotedialog.cpp
    operationdialog.cpp
    flatpakbackend.cpp
    flatpakjob.cpp
    appstream.cpp
    iconcache.cpp
  LINK
    tdeio-shared
    tdeui-shared
    ${FLATPAK_LIBRARIES}
    ${GIO_LIBRARIES}
    ${ZLIB_LIBRARIES}
  DESTINATION ${BIN_INSTALL_DIR}
)


##### other data ################################

install( FILES tdeflatpak.desktop DESTINATION ${XDG_APPS_INSTALL_DIR} )
