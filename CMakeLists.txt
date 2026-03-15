cmake_minimum_required(VERSION 3.16)
project(ModelingMethodsLab1 VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(APPLE)
    if(EXISTS "/opt/homebrew/opt/qtbase/lib/cmake/Qt6")
        list(PREPEND CMAKE_PREFIX_PATH "/opt/homebrew/opt/qtbase")
        if(EXISTS "/opt/homebrew/opt/qtsvg/lib/cmake/Qt6Svg")
            set(Qt6Svg_DIR "/opt/homebrew/opt/qtsvg/lib/cmake/Qt6Svg" CACHE PATH "Qt6Svg config dir")
            list(PREPEND CMAKE_PREFIX_PATH "/opt/homebrew/opt/qtsvg")
        endif()
        if(EXISTS "/opt/homebrew/opt/qtcharts/lib/cmake/Qt6Charts")
            set(Qt6Charts_DIR "/opt/homebrew/opt/qtcharts/lib/cmake/Qt6Charts" CACHE PATH "Qt6Charts config dir")
            list(PREPEND CMAKE_PREFIX_PATH "/opt/homebrew/opt/qtcharts")
        endif()
    elseif(EXISTS "/usr/local/opt/qtbase/lib/cmake/Qt6")
        list(PREPEND CMAKE_PREFIX_PATH "/usr/local/opt/qtbase")
        if(EXISTS "/usr/local/opt/qtsvg/lib/cmake/Qt6Svg")
            set(Qt6Svg_DIR "/usr/local/opt/qtsvg/lib/cmake/Qt6Svg" CACHE PATH "Qt6Svg config dir")
            list(PREPEND CMAKE_PREFIX_PATH "/usr/local/opt/qtsvg")
        endif()
        if(EXISTS "/usr/local/opt/qtcharts/lib/cmake/Qt6Charts")
            set(Qt6Charts_DIR "/usr/local/opt/qtcharts/lib/cmake/Qt6Charts" CACHE PATH "Qt6Charts config dir")
            list(PREPEND CMAKE_PREFIX_PATH "/usr/local/opt/qtcharts")
        endif()
    endif()
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets Svg)
find_package(Qt6Charts REQUIRED)

add_executable(
    ModelingMethodsLab1
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
)

target_link_libraries(ModelingMethodsLab1 PRIVATE Qt6::Widgets Qt6::Svg Qt6::Charts)

if(APPLE)
    set(QTBASE_PLATFORMS_PLUGIN "")
    set(QTSVG_ICONENGINE_PLUGIN "")

    if(EXISTS "/opt/homebrew/opt/qtbase/share/qt/plugins/platforms/libqcocoa.dylib")
        set(QTBASE_PLATFORMS_PLUGIN "/opt/homebrew/opt/qtbase/share/qt/plugins/platforms/libqcocoa.dylib")
    elseif(EXISTS "/usr/local/opt/qtbase/share/qt/plugins/platforms/libqcocoa.dylib")
        set(QTBASE_PLATFORMS_PLUGIN "/usr/local/opt/qtbase/share/qt/plugins/platforms/libqcocoa.dylib")
    endif()

    if(EXISTS "/opt/homebrew/opt/qtsvg/share/qt/plugins/iconengines/libqsvgicon.dylib")
        set(QTSVG_ICONENGINE_PLUGIN "/opt/homebrew/opt/qtsvg/share/qt/plugins/iconengines/libqsvgicon.dylib")
    elseif(EXISTS "/usr/local/opt/qtsvg/share/qt/plugins/iconengines/libqsvgicon.dylib")
        set(QTSVG_ICONENGINE_PLUGIN "/usr/local/opt/qtsvg/share/qt/plugins/iconengines/libqsvgicon.dylib")
    endif()

    if(QTBASE_PLATFORMS_PLUGIN)
        add_custom_command(
            TARGET ModelingMethodsLab1 POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:ModelingMethodsLab1>/plugins/platforms"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${QTBASE_PLATFORMS_PLUGIN}"
                "$<TARGET_FILE_DIR:ModelingMethodsLab1>/plugins/platforms/libqcocoa.dylib"
        )
    endif()

    if(QTSVG_ICONENGINE_PLUGIN)
        add_custom_command(
            TARGET ModelingMethodsLab1 POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:ModelingMethodsLab1>/plugins/iconengines"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${QTSVG_ICONENGINE_PLUGIN}"
                "$<TARGET_FILE_DIR:ModelingMethodsLab1>/plugins/iconengines/libqsvgicon.dylib"
        )
    endif()

    file(GENERATE
        OUTPUT "$<TARGET_FILE_DIR:ModelingMethodsLab1>/qt.conf"
        CONTENT "[Paths]\nPlugins = plugins\n"
    )
endif()
