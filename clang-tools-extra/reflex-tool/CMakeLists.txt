set(LLVM_LINK_COMPONENTS support)

add_clang_tool(reflex-tool
        tool/reflex-tool.cpp
        tool/alias_finder.cpp
        tool/alias_finder.hpp
        tool/class_finder.cpp
        tool/class_finder.hpp
        tool/enum_finder.cpp
        tool/enum_finder.hpp
        tool/function_finder.cpp
        tool/function_finder.hpp
        tool/json_writer.cpp
        tool/json_writer.hpp
        tool/common.cpp
        tool/common.hpp
        )
target_link_libraries(reflex-tool
        PRIVATE
        clangTooling
        clangBasic
        clangASTMatchers
        )

target_compile_features(reflex-tool PRIVATE cxx_std_20)

set(reflexToolCmakeInstallPath "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/cmake/reflex-tool/ReflexTool.cmake")
add_custom_target(
        install-reflex-tool-cmake
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/ReflexTool.cmake"
                "${reflexToolCmakeInstallPath}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/ReflexTool.cmake"
        COMMENT "Installing ${reflexToolCmakeInstallPath}"
        VERBATIM
)

set(reflexToolConfigBuildPath "${CMAKE_CURRENT_BINARY_DIR}/reflex-toolConfig.cmake")
set(reflexToolConfigInstallPath "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/cmake/reflex-tool/reflex-toolConfig.cmake")
include(CMakePackageConfigHelpers)
configure_package_config_file(
        reflex-toolConfig.cmake.in
        "${reflexToolConfigBuildPath}"
        INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/reflex-tool"
)
add_custom_target(
        install-reflex-tool-config-cmake
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${reflexToolConfigBuildPath}" "${reflexToolConfigInstallPath}"
        DEPENDS "${reflexToolConfigBuildPath}"
        COMMENT "Installing ${reflexToolConfigInstallPath}"
        VERBATIM
)
