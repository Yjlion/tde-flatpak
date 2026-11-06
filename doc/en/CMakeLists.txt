# Handbook for KHelpCenter (help:/tdeflatpak).  The screenshots are shared
# with the README and live in the top-level screenshots/ directory.
tde_create_handbook(
  DESTINATION tdeflatpak
  EXTRA
    ${CMAKE_SOURCE_DIR}/screenshots/browse.png
    ${CMAKE_SOURCE_DIR}/screenshots/details.png
    ${CMAKE_SOURCE_DIR}/screenshots/permissions.png
    ${CMAKE_SOURCE_DIR}/screenshots/kcm.png
    ${CMAKE_SOURCE_DIR}/screenshots/installed.png
    ${CMAKE_SOURCE_DIR}/screenshots/search.png
    ${CMAKE_SOURCE_DIR}/screenshots/updates.png
    ${CMAKE_SOURCE_DIR}/screenshots/operation.png
    ${CMAKE_SOURCE_DIR}/screenshots/remotes.png
    ${CMAKE_SOURCE_DIR}/screenshots/add-remote.png
)
