find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Concurrent)
if(BUILD_TESTING)
    find_package(Qt6 6.8 REQUIRED COMPONENTS Test)
endif()
qt_standard_project_setup()

# Button and ToolButton fade when they cannot be used. Other controls stay Basic.
add_subdirectory(controls)
if(SPENTIT_SHEET_PROTOTYPE)
qt_add_library(spentit_sheet_model STATIC sheet_model.cpp sheet_model.hpp)
target_include_directories(spentit_sheet_model PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(spentit_sheet_model PUBLIC spentit_sheet_session Qt6::Core PRIVATE Qt6::Concurrent)
spentit_compiler_options(spentit_sheet_model)
qt_add_executable(spentit_sheet_prototype main.cpp)
target_link_libraries(spentit_sheet_prototype PRIVATE spentit_sheet_model Qt6::Quick Qt6::QuickControls2)
qt_add_resources(spentit_sheet_prototype "sheet_ui" PREFIX "/spentit" FILES Main.qml ClearableSearchField.qml NavigableTextField.qml)
spentit_compiler_options(spentit_sheet_prototype)
if(BUILD_TESTING)
    add_executable(spentit_sheet_tests ../../tests/app/sheet_tests.cpp)
    target_link_libraries(spentit_sheet_tests PRIVATE spentit_sheet_session)
    spentit_compiler_options(spentit_sheet_tests)
    add_test(NAME sheet_session COMMAND spentit_sheet_tests)
    qt_add_executable(spentit_sheet_model_tests ../../tests/ui/sheet_model_tests.cpp)
    target_link_libraries(spentit_sheet_model_tests PRIVATE spentit_sheet_model Qt6::Test)
    spentit_compiler_options(spentit_sheet_model_tests)
    add_test(NAME sheet_model COMMAND spentit_sheet_model_tests)
    add_test(NAME sheet_qml_smoke COMMAND spentit_sheet_prototype --smoke)
    set_tests_properties(sheet_qml_smoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 30)
endif()
if(BUILD_TESTING)
    qt_add_executable(spentit_sheet_interaction_tests ../../tests/ui/sheet_interaction_tests.cpp)
    target_link_libraries(spentit_sheet_interaction_tests PRIVATE spentit_sheet_model Qt6::Quick Qt6::QuickControls2 Qt6::Test)
    qt_add_resources(spentit_sheet_interaction_tests "interaction_ui" PREFIX "/spentit" FILES Main.qml ClearableSearchField.qml NavigableTextField.qml)
    spentit_compiler_options(spentit_sheet_interaction_tests)
    add_test(NAME sheet_interaction COMMAND spentit_sheet_interaction_tests)
    set_tests_properties(sheet_interaction PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 30)
endif()
add_executable(spentit_sheet_benchmark ../../tests/app/sheet_benchmark.cpp)
target_link_libraries(spentit_sheet_benchmark PRIVATE spentit_sheet_session)
spentit_compiler_options(spentit_sheet_benchmark)

endif()
if(SPENTIT_IMPORT_UI)
  find_package(Python3 COMPONENTS Interpreter REQUIRED)
  set(SPENTIT_HELP_BUNDLE "${CMAKE_CURRENT_BINARY_DIR}/help-bundle")
  set(SPENTIT_HELP_SAMPLE_BUNDLE "${CMAKE_CURRENT_BINARY_DIR}/help-sample-bundle")
  file(GLOB_RECURSE SPENTIT_HELP_SOURCES CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/manual/*.md" "${PROJECT_SOURCE_DIR}/manual/images/*.png")
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${SPENTIT_HELP_SOURCES}
    "${CMAKE_CURRENT_SOURCE_DIR}/ImportMain.qml"
    "${CMAKE_CURRENT_SOURCE_DIR}/ReportsWorkspace.qml"
    "${PROJECT_SOURCE_DIR}/tools/prepare_help.py"
    "${PROJECT_SOURCE_DIR}/manual/index.md"
    "${PROJECT_SOURCE_DIR}/tests/fixtures/help/sample/start.md"
    "${PROJECT_SOURCE_DIR}/tests/fixtures/help/sample/details.md"
    "${PROJECT_SOURCE_DIR}/tests/fixtures/help/images/sample-mark.png")
  execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/prepare_help.py"
            --manual "${PROJECT_SOURCE_DIR}/manual" --output "${SPENTIT_HELP_BUNDLE}"
            --context-source "${CMAKE_CURRENT_SOURCE_DIR}/ImportMain.qml"
            --context-source "${CMAKE_CURRENT_SOURCE_DIR}/ReportsWorkspace.qml"
    RESULT_VARIABLE spentit_help_prepare_result
    ERROR_VARIABLE spentit_help_prepare_error)
  if(NOT spentit_help_prepare_result EQUAL 0)
    message(FATAL_ERROR "Help bundle failed (${spentit_help_prepare_result}): ${spentit_help_prepare_error}")
  endif()
  execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/prepare_help.py"
            --manual "${PROJECT_SOURCE_DIR}/tests/fixtures/help" --output "${SPENTIT_HELP_SAMPLE_BUNDLE}"
    RESULT_VARIABLE spentit_help_sample_prepare_result
    ERROR_VARIABLE spentit_help_sample_prepare_error)
  if(NOT spentit_help_sample_prepare_result EQUAL 0)
    message(FATAL_ERROR "Help sample bundle failed (${spentit_help_sample_prepare_result}): ${spentit_help_sample_prepare_error}")
  endif()
  file(GLOB_RECURSE SPENTIT_HELP_BUNDLE_FILES CONFIGURE_DEPENDS "${SPENTIT_HELP_BUNDLE}/*")
  file(GLOB_RECURSE SPENTIT_HELP_SAMPLE_BUNDLE_FILES CONFIGURE_DEPENDS "${SPENTIT_HELP_SAMPLE_BUNDLE}/*")
  set_source_files_properties(help/HelpWindow.qml PROPERTIES QT_RESOURCE_ALIAS "HelpWindow.qml")
  qt_add_library(spentit_help STATIC help/help_catalog.cpp help/help_catalog.hpp)
  target_include_directories(spentit_help PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries(spentit_help PUBLIC Qt6::Core Qt6::Gui Qt6::Quick)
  spentit_compiler_options(spentit_help)
  set(SPENTIT_ABOUT_BUNDLE "${CMAKE_CURRENT_BINARY_DIR}/about-bundle")
  file(MAKE_DIRECTORY "${SPENTIT_ABOUT_BUNDLE}")
  file(GLOB SPENTIT_ABOUT_NOTICES CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/third_party/notices/*")
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${SPENTIT_ABOUT_NOTICES} "${PROJECT_SOURCE_DIR}/tools/prepare_about.py"
    "${PROJECT_SOURCE_DIR}/third_party/sqlcipher/LICENSE.md"
    "${PROJECT_SOURCE_DIR}/third_party/pcre2/LICENSE.md")
  execute_process(COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/prepare_about.py"
    --notices "${PROJECT_SOURCE_DIR}/third_party/notices" --output "${SPENTIT_ABOUT_BUNDLE}/components.json"
    --qt-version "${Qt6Core_VERSION}" --platform "${CMAKE_SYSTEM_NAME}" --qt-cmake-dir "${Qt6Core_DIR}"
    RESULT_VARIABLE about_result)
  if(NOT about_result EQUAL 0)
    message(FATAL_ERROR "Cannot prepare offline About notices")
  endif()
  add_custom_target(spentit_about_build
    COMMAND "${CMAKE_COMMAND}" "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}" "-DVERSION=${PROJECT_VERSION}"
      "-DCONFIG=$<CONFIG>" "-DOUTPUT=${SPENTIT_ABOUT_BUNDLE}/about_build.hpp"
      -P "${PROJECT_SOURCE_DIR}/tools/about_build.cmake"
    BYPRODUCTS "${SPENTIT_ABOUT_BUNDLE}/about_build.hpp" VERBATIM)
  qt_add_library(spentit_import_model STATIC import_model.cpp import_model.hpp file_exchange_codec.cpp file_exchange_codec.hpp about/about_info.cpp about/about_info.hpp)
  add_dependencies(spentit_import_model spentit_about_build)
  target_include_directories(spentit_import_model PRIVATE "${SPENTIT_ABOUT_BUNDLE}")
  set_source_files_properties(about/terms.txt PROPERTIES QT_RESOURCE_ALIAS "terms.txt")
  set_source_files_properties("${SPENTIT_ABOUT_BUNDLE}/components.json" PROPERTIES QT_RESOURCE_ALIAS "components.json")
  qt_add_resources(spentit_import_model "about_notices" PREFIX "/spentit/about"
    FILES about/terms.txt "${SPENTIT_ABOUT_BUNDLE}/components.json")
  set_source_files_properties(about/AboutWindow.qml PROPERTIES QT_RESOURCE_ALIAS "AboutWindow.qml")
  target_include_directories(spentit_import_model PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries(spentit_import_model PUBLIC spentit_import_view Qt6::Core Qt6::Gui PRIVATE Qt6::Concurrent)
  spentit_compiler_options(spentit_import_model)
  qt_add_executable(spentit_import_experiment import_main.cpp)
  set(SPENTIT_APPLICATION_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" CACHE PATH
      "Directory for the SpentIt application executable")
  set_target_properties(spentit_import_experiment PROPERTIES
      OUTPUT_NAME SpentIt
      RUNTIME_OUTPUT_DIRECTORY "${SPENTIT_APPLICATION_OUTPUT_DIRECTORY}")
  target_link_libraries(spentit_import_experiment PRIVATE spentit_import_model spentit_import_store spentit_help Qt6::Quick Qt6::QuickControls2)
  qt_add_resources(spentit_import_experiment "import_ui" PREFIX "/spentit" FILES ImportMain.qml about/AboutWindow.qml ClearableSearchField.qml NavigableTextField.qml CategoryCompletion.qml Theme.qml CalendarPicker.qml ReportsWorkspace.qml ReportChart.qml help/HelpWindow.qml)
  qt_add_resources(spentit_import_experiment "help_bundle" PREFIX "/spentit/help" BASE "${SPENTIT_HELP_BUNDLE}" FILES ${SPENTIT_HELP_BUNDLE_FILES})
  spentit_compiler_options(spentit_import_experiment)
  qt_add_executable(spentit_import_benchmark ../../tests/ui/import_benchmark.cpp)
  target_link_libraries(spentit_import_benchmark PRIVATE spentit_import_model spentit_import_store Qt6::Quick Qt6::QuickControls2)
  qt_add_resources(spentit_import_benchmark "import_benchmark_ui" PREFIX "/spentit" FILES ImportMain.qml about/AboutWindow.qml ClearableSearchField.qml NavigableTextField.qml CategoryCompletion.qml Theme.qml CalendarPicker.qml ReportsWorkspace.qml ReportChart.qml)
  spentit_compiler_options(spentit_import_benchmark)
  if(BUILD_TESTING)
    qt_add_executable(spentit_file_exchange_tests ../../tests/ui/file_exchange_tests.cpp)
    target_link_libraries(spentit_file_exchange_tests PRIVATE spentit_import_model spentit_import_store spentit_sqlcipher Qt6::Quick Qt6::QuickControls2 Qt6::Test)
    qt_add_resources(spentit_file_exchange_tests "exchange_test_ui" PREFIX "/spentit" FILES ImportMain.qml about/AboutWindow.qml ClearableSearchField.qml NavigableTextField.qml CategoryCompletion.qml Theme.qml CalendarPicker.qml ReportsWorkspace.qml ReportChart.qml)
    spentit_compiler_options(spentit_file_exchange_tests)
    add_test(NAME import.file_exchange COMMAND spentit_file_exchange_tests)
    set_tests_properties(import.file_exchange PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 180)
    qt_add_executable(spentit_report_ui_tests ../../tests/ui/report_ui_tests.cpp)
    target_link_libraries(spentit_report_ui_tests PRIVATE spentit_import_model spentit_import_store Qt6::Quick Qt6::QuickControls2 Qt6::Test)
    qt_add_resources(spentit_report_ui_tests "report_test_ui" PREFIX "/spentit" FILES ImportMain.qml about/AboutWindow.qml ClearableSearchField.qml NavigableTextField.qml CategoryCompletion.qml Theme.qml CalendarPicker.qml ReportsWorkspace.qml ReportChart.qml)
    spentit_compiler_options(spentit_report_ui_tests)
    add_test(NAME import.reports_ui COMMAND spentit_report_ui_tests)
    set_tests_properties(import.reports_ui PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 120)
    qt_add_executable(spentit_import_ui_tests ../../tests/ui/import_model_tests.cpp)
    target_compile_definitions(spentit_import_ui_tests PRIVATE SPENTIT_REVIEW_FIXTURE="${PROJECT_SOURCE_DIR}/tests/fixtures/qif/durable-review-synthetic.qif")
    target_link_libraries(spentit_import_ui_tests PRIVATE spentit_import_model spentit_import_store spentit_sqlcipher Qt6::Quick Qt6::QuickControls2 Qt6::Test)
    qt_add_resources(spentit_import_ui_tests "import_test_ui" PREFIX "/spentit" FILES ImportMain.qml about/AboutWindow.qml ClearableSearchField.qml NavigableTextField.qml CategoryCompletion.qml Theme.qml CalendarPicker.qml ReportsWorkspace.qml ReportChart.qml)
    spentit_compiler_options(spentit_import_ui_tests)
    add_test(NAME import.action_undo_ui COMMAND spentit_import_ui_tests commonUndoDirtyTargetAndTextUndo)
    set_tests_properties(import.action_undo_ui PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 60)
    add_test(NAME import.ui COMMAND spentit_import_ui_tests)
    set_tests_properties(import.ui PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 180)
    qt_add_executable(spentit_import_navigation_tests ../../tests/ui/import_navigation_tests.cpp)
    target_link_libraries(spentit_import_navigation_tests PRIVATE spentit_import_model spentit_import_store Qt6::Quick Qt6::QuickControls2 Qt6::Test)
    qt_add_resources(spentit_import_navigation_tests "navigation_ui" PREFIX "/spentit" FILES ImportMain.qml about/AboutWindow.qml ClearableSearchField.qml NavigableTextField.qml CategoryCompletion.qml Theme.qml CalendarPicker.qml ReportsWorkspace.qml ReportChart.qml)
    spentit_compiler_options(spentit_import_navigation_tests)
    add_test(NAME import.navigation COMMAND spentit_import_navigation_tests)
    set_tests_properties(import.navigation PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 90)
    qt_add_executable(spentit_import_recovery_tests ../../tests/ui/import_recovery_tests.cpp)
    target_link_libraries(spentit_import_recovery_tests PRIVATE spentit_import_model spentit_import_store Qt6::Quick Qt6::QuickControls2 Qt6::Test)
    qt_add_resources(spentit_import_recovery_tests "recovery_ui" PREFIX "/spentit" FILES ImportMain.qml about/AboutWindow.qml ClearableSearchField.qml NavigableTextField.qml CategoryCompletion.qml Theme.qml CalendarPicker.qml ReportsWorkspace.qml ReportChart.qml)
    spentit_compiler_options(spentit_import_recovery_tests)
    add_test(NAME import.recovery_ui COMMAND spentit_import_recovery_tests)
    set_tests_properties(import.recovery_ui PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 90)
    qt_add_executable(spentit_committed_ui_tests ../../tests/ui/committed_ui_tests.cpp)
    target_link_libraries(spentit_committed_ui_tests PRIVATE spentit_import_model spentit_import_store Qt6::Quick Qt6::QuickControls2 Qt6::Test)
    qt_add_resources(spentit_committed_ui_tests "committed_ui" PREFIX "/spentit" FILES ImportMain.qml about/AboutWindow.qml ClearableSearchField.qml NavigableTextField.qml CategoryCompletion.qml Theme.qml CalendarPicker.qml ReportsWorkspace.qml ReportChart.qml)
    spentit_compiler_options(spentit_committed_ui_tests)
    add_test(NAME import.committed_ui COMMAND spentit_committed_ui_tests)
    set_tests_properties(import.committed_ui PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 180)
    qt_add_executable(spentit_category_ui_tests ../../tests/ui/category_ui_tests.cpp)
    target_link_libraries(spentit_category_ui_tests PRIVATE spentit_import_model spentit_import_store Qt6::Quick Qt6::QuickControls2 Qt6::Test)
    qt_add_resources(spentit_category_ui_tests "category_ui" PREFIX "/spentit" FILES ImportMain.qml about/AboutWindow.qml ClearableSearchField.qml NavigableTextField.qml CategoryCompletion.qml Theme.qml CalendarPicker.qml ReportsWorkspace.qml ReportChart.qml)
    spentit_compiler_options(spentit_category_ui_tests)
    add_test(NAME import.category_ui COMMAND spentit_category_ui_tests)
    set_tests_properties(import.category_ui PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 180)
    if(SPENTIT_RULES)
      qt_add_executable(spentit_rule_ui_tests ../../tests/ui/rule_ui_tests.cpp)
      target_link_libraries(spentit_rule_ui_tests PRIVATE spentit_import_model spentit_import_store spentit_sqlcipher Qt6::Quick Qt6::QuickControls2 Qt6::Test)
      qt_add_resources(spentit_rule_ui_tests "rule_ui" PREFIX "/spentit" FILES ImportMain.qml about/AboutWindow.qml ClearableSearchField.qml NavigableTextField.qml CategoryCompletion.qml Theme.qml CalendarPicker.qml ReportsWorkspace.qml ReportChart.qml)
      spentit_compiler_options(spentit_rule_ui_tests)
      add_test(NAME import.rules_ui COMMAND spentit_rule_ui_tests)
      set_tests_properties(import.rules_ui PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 180)
      qt_add_executable(spentit_help_ui_tests ../../tests/ui/help_ui_tests.cpp)
      if(APPLE)
        enable_language(OBJCXX)
        target_sources(spentit_help_ui_tests PRIVATE ../../tests/ui/about_menu_macos.mm)
        target_link_libraries(spentit_help_ui_tests PRIVATE "-framework AppKit")
      endif()
      target_link_libraries(spentit_help_ui_tests PRIVATE spentit_import_model spentit_import_store spentit_sqlcipher spentit_help Qt6::Quick Qt6::QuickControls2 Qt6::Test)
      qt_add_resources(spentit_help_ui_tests "help_ui" PREFIX "/spentit" FILES ImportMain.qml about/AboutWindow.qml ClearableSearchField.qml NavigableTextField.qml CategoryCompletion.qml Theme.qml CalendarPicker.qml ReportsWorkspace.qml ReportChart.qml help/HelpWindow.qml)
      qt_add_resources(spentit_help_ui_tests "help_ui_bundle" PREFIX "/spentit/help" BASE "${SPENTIT_HELP_SAMPLE_BUNDLE}" FILES ${SPENTIT_HELP_SAMPLE_BUNDLE_FILES})
      qt_add_resources(spentit_help_ui_tests "help_ui_manual" PREFIX "/spentit/help-manual" BASE "${SPENTIT_HELP_BUNDLE}" FILES ${SPENTIT_HELP_BUNDLE_FILES})
      file(GLOB SPENTIT_HELP_MANUAL_IMAGES CONFIGURE_DEPENDS "${SPENTIT_HELP_BUNDLE}/images/*.png")
      qt_add_resources(spentit_help_ui_tests "help_ui_manual_images" PREFIX "/spentit/help" BASE "${SPENTIT_HELP_BUNDLE}" FILES ${SPENTIT_HELP_MANUAL_IMAGES})
      spentit_compiler_options(spentit_help_ui_tests)
      add_test(NAME import.about_ui COMMAND spentit_help_ui_tests aboutBeforeOpeningAndOfflineNotices aboutPreservesUnsavedTransaction)
      set_tests_properties(import.about_ui PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 60)
      add_test(NAME import.help_ui COMMAND spentit_help_ui_tests)
      # Full-manual reading, lookup and continuous financial walkthroughs.
      set_tests_properties(import.help_ui PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 600)
    endif()
    add_test(NAME help.prepare COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/ui/help_prepare_tests.py")
    add_test(NAME help.capture_manual_images COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tests/ui/capture_manual_images_tests.py")
    qt_add_executable(spentit_help_tests ../../tests/ui/help_catalog_tests.cpp)
    target_link_libraries(spentit_help_tests PRIVATE spentit_help Qt6::Test)
    target_compile_definitions(spentit_help_tests PRIVATE
      "SPENTIT_HELP_MANIFEST=\"${SPENTIT_HELP_SAMPLE_BUNDLE}/manifest.json\""
      "SPENTIT_HELP_MANUAL_MANIFEST=\"${SPENTIT_HELP_BUNDLE}/manifest.json\"")
    spentit_compiler_options(spentit_help_tests)
    add_test(NAME help.catalog COMMAND spentit_help_tests)
    qt_add_executable(spentit_controls_style_tests ../../tests/ui/controls_style_tests.cpp)
    target_link_libraries(spentit_controls_style_tests PRIVATE spentit_controls_style Qt6::Quick Qt6::QuickControls2 Qt6::Test)
    target_include_directories(spentit_controls_style_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    spentit_compiler_options(spentit_controls_style_tests)
    add_test(NAME import.controls_style COMMAND spentit_controls_style_tests)
    set_tests_properties(import.controls_style PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 30)
    add_test(NAME import.qml COMMAND spentit_import_experiment --smoke)
    set_tests_properties(import.qml PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 30)
    add_test(NAME import.group_benchmark_smoke COMMAND spentit_import_benchmark 120 --groups)
    set_tests_properties(import.group_benchmark_smoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 60)
    add_test(NAME import.benchmark_smoke COMMAND spentit_import_benchmark 100)
    set_tests_properties(import.benchmark_smoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 60)
  endif()
endif()

# Every executable rendering ImportMain.qml needs the same embedded artwork.
if(SPENTIT_IMPORT_UI)
  set(spentit_icon_files)
  foreach(variant light dark)
    foreach(size 16 32 64 128 256 512 1024)
      list(APPEND spentit_icon_files "${PROJECT_SOURCE_DIR}/assets/images/SpentIt_${variant}_${size}.png")
    endforeach()
  endforeach()
  get_property(spentit_ui_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
  foreach(target IN LISTS spentit_ui_targets)
    if(target MATCHES "^spentit_(import_(experiment|benchmark|ui_tests|navigation_tests|recovery_tests)|committed_ui_tests|category_ui_tests|rule_ui_tests|report_ui_tests|file_exchange_tests|help_ui_tests)$")
      qt_add_resources(${target} "${target}_icons" PREFIX "/spentit/images"
        BASE "${PROJECT_SOURCE_DIR}/assets/images" FILES ${spentit_icon_files})
    endif()
  endforeach()
  include("${PROJECT_SOURCE_DIR}/cmake/LocalPackaging.cmake")
endif()

# Every window that shows controls uses the same unavailable-button style.
foreach(target IN ITEMS
    spentit_sheet_prototype
    spentit_sheet_interaction_tests
    spentit_import_experiment
    spentit_import_benchmark
    spentit_file_exchange_tests
    spentit_report_ui_tests
    spentit_import_ui_tests
    spentit_import_navigation_tests
    spentit_import_recovery_tests
    spentit_committed_ui_tests
    spentit_category_ui_tests
    spentit_rule_ui_tests
    spentit_help_ui_tests
    spentit_macos_bundle
    spentit_controls_style_tests)
  if(TARGET ${target})
    target_link_libraries(${target} PRIVATE spentit_controls_style)
  endif()
endforeach()

if(TARGET spentit_import_model)
 target_link_libraries(spentit_import_model PUBLIC spentit_diagnostics)
endif()

if(SPENTIT_IMPORT_UI AND SPENTIT_DIAGNOSTICS)
 find_package(Qt6 6.8 REQUIRED COMPONENTS Network)
 target_sources(spentit_import_experiment PRIVATE diagnostic_transport.cpp diagnostic_transport.hpp)
 target_link_libraries(spentit_import_experiment PRIVATE Qt6::Network)
 if(TARGET spentit_macos_bundle)
   target_sources(spentit_macos_bundle PRIVATE diagnostic_transport.cpp diagnostic_transport.hpp)
   target_link_libraries(spentit_macos_bundle PRIVATE Qt6::Network)
 endif()
 qt_add_executable(spentit_diagnostics_cli ../../tools/diagnostics.cpp)
 set_target_properties(spentit_diagnostics_cli PROPERTIES OUTPUT_NAME spentit-diagnostics)
 target_link_libraries(spentit_diagnostics_cli PRIVATE Qt6::Core Qt6::Network)
 spentit_compiler_options(spentit_diagnostics_cli)
endif()

if(SPENTIT_IMPORT_UI AND SPENTIT_DIAGNOSTICS AND BUILD_TESTING)
 qt_add_executable(spentit_diagnostic_transport_tests ../../tests/diagnostics/transport_tests.cpp diagnostic_transport.cpp)
 target_include_directories(spentit_diagnostic_transport_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
 target_link_libraries(spentit_diagnostic_transport_tests PRIVATE spentit_diagnostics Qt6::Core Qt6::Network)
 spentit_compiler_options(spentit_diagnostic_transport_tests)
 add_test(NAME diagnostics.transport COMMAND spentit_diagnostic_transport_tests)
 set_tests_properties(diagnostics.transport PROPERTIES TIMEOUT 30)
 add_test(NAME diagnostics.qml COMMAND spentit_import_experiment --smoke --diagnostics)
 set_tests_properties(diagnostics.qml PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 30)
 add_test(NAME diagnostics.workload COMMAND spentit_import_benchmark 120 --groups)
 set_tests_properties(diagnostics.workload PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;SPENTIT_BENCHMARK_DIAGNOSTICS=1" TIMEOUT 90)
endif()
