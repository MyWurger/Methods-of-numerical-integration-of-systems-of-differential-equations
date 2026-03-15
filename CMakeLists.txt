cmake_minimum_required(VERSION 3.16)
project(ModelingMethodsLab1 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

function(_append_existing_prefixes out_var)
    set(prefixes ${${out_var}})
    foreach(path IN LISTS ARGN)
        if(path AND EXISTS "${path}")
            list(APPEND prefixes "${path}")
        endif()
    endforeach()
    list(REMOVE_DUPLICATES prefixes)
    set(${out_var} "${prefixes}" PARENT_SCOPE)
endfunction()

if(APPLE)
    set(_qt_candidate_prefixes "")

    find_program(_qt_qmake_executable NAMES qmake6 qmake)
    if(_qt_qmake_executable)
        get_filename_component(_qt_bin_dir "${_qt_qmake_executable}" DIRECTORY)
        get_filename_component(_qt_prefix "${_qt_bin_dir}" DIRECTORY)
        list(APPEND _qt_candidate_prefixes "${_qt_prefix}")
    endif()

    file(GLOB _qt_installer_prefixes LIST_DIRECTORIES true
        "/Applications/Qt/*/macos"
        "$ENV{HOME}/Qt/*/macos"
    )

    _append_existing_prefixes(
        _qt_candidate_prefixes
        ${_qt_installer_prefixes}
        "/opt/homebrew/opt/qtbase"
        "/opt/homebrew/opt/qtsvg"
        "/opt/homebrew/opt/qtcharts"
        "/usr/local/opt/qtbase"
        "/usr/local/opt/qtsvg"
        "/usr/local/opt/qtcharts"
    )

    if(_qt_candidate_prefixes)
        list(PREPEND CMAKE_PREFIX_PATH ${_qt_candidate_prefixes})
        list(REMOVE_DUPLICATES CMAKE_PREFIX_PATH)
    endif()
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(Qt6Svg REQUIRED)
find_package(Qt6Charts REQUIRED)

add_executable(
    ModelingMethodsLab1
    MACOSX_BUNDLE
    main.cpp
    include/MainWindow.h
    include/PlaceholderPlotWidget.h
    include/PlotChartWidget.h
    include/SimulationTypes.h
    include/TAbstractIntegrator.h
    include/TDynamicModel.h
    include/TEuler.h
    include/TRungeKutta.h
    include/TSpaceCraft.h
    include/TVector.h
    src/MainWindow.cpp
    src/PlaceholderPlotWidget.cpp
    src/PlotChartWidget.cpp
    src/TAbstractIntegrator.cpp
    src/TDynamicModel.cpp
    src/TEuler.cpp
    src/TRungeKutta.cpp
    src/TSpaceCraft.cpp
    resources/resources.qrc
)

target_include_directories(ModelingMethodsLab1 PRIVATE include)
target_compile_definitions(
    ModelingMethodsLab1
    PRIVATE MODELING_METHODS_LAB1_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)

set_target_properties(
    ModelingMethodsLab1
    PROPERTIES
        AUTOMOC ON
        AUTOUIC ON
        AUTORCC ON
        MACOSX_BUNDLE_BUNDLE_NAME "ModelingMethodsLab1"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.modelingmethods.lab1"
)

target_link_libraries(ModelingMethodsLab1 PRIVATE Qt6::Widgets Qt6::Svg Qt6::Charts)

if(COMMAND qt_finalize_executable)
    qt_finalize_executable(ModelingMethodsLab1)
endif()

if(APPLE)
    find_program(CODESIGN_EXECUTABLE NAMES codesign)
    find_program(
        MACDEPLOYQT_EXECUTABLE
        NAMES macdeployqt
        HINTS "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_BINS}"
    )

    set(QTBASE_PLATFORMS_PLUGIN "")
    if(QT6_INSTALL_PREFIX AND QT6_INSTALL_PLUGINS)
        set(QTBASE_PLATFORMS_PLUGIN
            "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_PLUGINS}/platforms/libqcocoa.dylib")
    endif()

    set(QTSVG_ICONENGINE_PLUGIN "")
    if(Qt6Svg_DIR)
        get_filename_component(_qt_svg_prefix "${Qt6Svg_DIR}/../../.." ABSOLUTE)
        if(EXISTS "${_qt_svg_prefix}/share/qt/plugins/iconengines/libqsvgicon.dylib")
            set(QTSVG_ICONENGINE_PLUGIN
                "${_qt_svg_prefix}/share/qt/plugins/iconengines/libqsvgicon.dylib")
        elseif(QT6_INSTALL_PREFIX AND QT6_INSTALL_PLUGINS
                AND EXISTS "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_PLUGINS}/iconengines/libqsvgicon.dylib")
            set(QTSVG_ICONENGINE_PLUGIN
                "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_PLUGINS}/iconengines/libqsvgicon.dylib")
        endif()
    endif()

    if(MACDEPLOYQT_EXECUTABLE)
        add_custom_command(
            TARGET ModelingMethodsLab1 POST_BUILD
            COMMAND "${MACDEPLOYQT_EXECUTABLE}"
                "$<TARGET_BUNDLE_DIR:ModelingMethodsLab1>"
                -no-plugins
                -always-overwrite
            COMMENT "Deploying Qt frameworks into the macOS app bundle"
        )
    endif()

    if(EXISTS "${QTBASE_PLATFORMS_PLUGIN}")
        add_custom_command(
            TARGET ModelingMethodsLab1 POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_BUNDLE_DIR:ModelingMethodsLab1>/Contents/PlugIns/platforms"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${QTBASE_PLATFORMS_PLUGIN}"
                "$<TARGET_BUNDLE_DIR:ModelingMethodsLab1>/Contents/PlugIns/platforms/libqcocoa.dylib"
            COMMENT "Copying the Cocoa platform plugin into the macOS app bundle"
        )
    endif()

    if(EXISTS "${QTSVG_ICONENGINE_PLUGIN}")
        add_custom_command(
            TARGET ModelingMethodsLab1 POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_BUNDLE_DIR:ModelingMethodsLab1>/Contents/PlugIns/iconengines"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${QTSVG_ICONENGINE_PLUGIN}"
                "$<TARGET_BUNDLE_DIR:ModelingMethodsLab1>/Contents/PlugIns/iconengines/libqsvgicon.dylib"
            COMMENT "Copying the SVG icon engine plugin into the macOS app bundle"
        )
    endif()

    if(CODESIGN_EXECUTABLE)
        add_custom_command(
            TARGET ModelingMethodsLab1 POST_BUILD
            COMMAND "${CODESIGN_EXECUTABLE}"
                --force
                --deep
                --sign
                -
                "$<TARGET_BUNDLE_DIR:ModelingMethodsLab1>"
            COMMENT "Applying an ad-hoc signature to the macOS app bundle"
        )
    endif()
endif()
