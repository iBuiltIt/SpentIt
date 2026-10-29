# Local packaging only; the regular ./build/SpentIt executable stays available.
if(APPLE)
  # Qt Quick deployment includes SVG plugins. Make their split-install framework
  # discoverable from the bundle itself before macdeployqt scans dependencies.
  find_package(Qt6 6.8 REQUIRED COMPONENTS Svg)
  qt_add_executable(spentit_macos_bundle MACOSX_BUNDLE EXCLUDE_FROM_ALL import_main.cpp)
  target_link_libraries(spentit_macos_bundle PRIVATE spentit_import_model spentit_import_store Qt6::Quick Qt6::QuickControls2 Qt6::Svg)
  qt_add_resources(spentit_macos_bundle "bundle_ui" PREFIX "/spentit" FILES ImportMain.qml)
  qt_add_resources(spentit_macos_bundle "bundle_icons" PREFIX "/spentit/images"
    BASE "${PROJECT_SOURCE_DIR}/assets/images" FILES ${spentit_icon_files})
  set(icon "${PROJECT_SOURCE_DIR}/assets/images/SpentIt_light.icns")
  target_sources(spentit_macos_bundle PRIVATE "${icon}")
  set_source_files_properties("${icon}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
  set_target_properties(spentit_macos_bundle PROPERTIES
    OUTPUT_NAME SpentIt
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/local-app"
    MACOSX_BUNDLE_BUNDLE_NAME SpentIt
    MACOSX_BUNDLE_GUI_IDENTIFIER "local.spentit.desktop"
    MACOSX_BUNDLE_ICON_FILE SpentIt_light.icns
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}")
  spentit_compiler_options(spentit_macos_bundle)
elseif(WIN32)
  enable_language(RC)
  configure_file("${PROJECT_SOURCE_DIR}/packaging/SpentIt.rc.in" "${CMAKE_CURRENT_BINARY_DIR}/SpentIt.rc" @ONLY)
  target_sources(spentit_import_experiment PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/SpentIt.rc")
  set_target_properties(spentit_import_experiment PROPERTIES WIN32_EXECUTABLE TRUE)
else()
  include(GNUInstallDirs)
  install(TARGETS spentit_import_experiment RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT SpentIt)
  install(FILES "${PROJECT_SOURCE_DIR}/packaging/SpentIt.desktop"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/applications" COMPONENT SpentIt)
  foreach(size 16 32 64 128 256 512 1024)
    install(FILES "${PROJECT_SOURCE_DIR}/assets/images/SpentIt_light_${size}.png"
      DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/${size}x${size}/apps"
      RENAME SpentIt.png COMPONENT SpentIt)
  endforeach()
endif()
