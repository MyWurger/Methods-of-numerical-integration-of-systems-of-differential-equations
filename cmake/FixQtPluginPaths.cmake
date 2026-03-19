if(NOT DEFINED PLUGIN OR PLUGIN STREQUAL "")
    message(FATAL_ERROR "PLUGIN path is not provided")
endif()

if(NOT DEFINED INSTALL_NAME_TOOL_EXECUTABLE OR INSTALL_NAME_TOOL_EXECUTABLE STREQUAL "")
    message(FATAL_ERROR "INSTALL_NAME_TOOL_EXECUTABLE is not provided")
endif()

string(REPLACE "\\ " " " PLUGIN "${PLUGIN}")
string(REPLACE "\\ " " " INSTALL_NAME_TOOL_EXECUTABLE "${INSTALL_NAME_TOOL_EXECUTABLE}")

find_program(OTOOL_EXECUTABLE NAMES otool REQUIRED)

execute_process(
    COMMAND "${OTOOL_EXECUTABLE}" -L "${PLUGIN}"
    OUTPUT_VARIABLE plugin_deps
    RESULT_VARIABLE otool_result
)

if(NOT otool_result EQUAL 0)
    message(FATAL_ERROR "Failed to inspect plugin dependencies: ${PLUGIN}")
endif()

set(app_qtgui "@executable_path/../Frameworks/QtGui.framework/Versions/A/QtGui")
set(app_qtcore "@executable_path/../Frameworks/QtCore.framework/Versions/A/QtCore")

function(_rewrite_if_present old_path new_path)
    if(plugin_deps MATCHES "${old_path}")
        execute_process(
            COMMAND "${INSTALL_NAME_TOOL_EXECUTABLE}" -change "${old_path}" "${new_path}" "${PLUGIN}"
            RESULT_VARIABLE rewrite_result
        )
        if(NOT rewrite_result EQUAL 0)
            message(FATAL_ERROR "Failed to rewrite dependency ${old_path} in ${PLUGIN}")
        endif()
    endif()
endfunction()

_rewrite_if_present("/opt/homebrew/opt/qtbase/lib/QtGui.framework/Versions/A/QtGui" "${app_qtgui}")
_rewrite_if_present("/usr/local/opt/qtbase/lib/QtGui.framework/Versions/A/QtGui" "${app_qtgui}")
_rewrite_if_present("/opt/homebrew/opt/qtbase/lib/QtCore.framework/Versions/A/QtCore" "${app_qtcore}")
_rewrite_if_present("/usr/local/opt/qtbase/lib/QtCore.framework/Versions/A/QtCore" "${app_qtcore}")
