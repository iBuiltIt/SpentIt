qt_add_qml_module(spentit_controls_style
    URI SpentItControls
    VERSION 1.0
    RESOURCE_PREFIX "/qt/qml"
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/SpentItControls"
    QML_FILES
        Button.qml
        ToolButton.qml)
