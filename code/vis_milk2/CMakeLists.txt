# MilkDrop3 Main Visualizer Executable

if(WIN32)
    # Windows build: compile all files as before
    set(MILKDROP3_SOURCES
        Milkdrop2PcmVisualizer.cpp
        plugin.cpp
        pluginshell.cpp
        dxcontext.cpp
        milkdropfs.cpp
        state.cpp
        menu.cpp
        support.cpp
        texmgr.cpp
        textmgr.cpp
        fft.cpp
        utility.cpp
        wasabi.cpp
    )

    set(MILKDROP3_AUDIO_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/../audio/audiobuf.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../audio/guid.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../audio/log.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../audio/prefs.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../audio/loopback-capture.cpp
    )

    add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_CRT_NON_CONFORMING_SWPRINTFS)

    add_executable(milkdrop3
        ${MILKDROP3_SOURCES}
        ${MILKDROP3_AUDIO_SOURCES}
        plugin.rc
    )

    target_include_directories(milkdrop3 PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_CURRENT_SOURCE_DIR}/../audio
    )

    target_link_libraries(milkdrop3 PRIVATE
        nseel2
        d3d9.lib d3dx9.lib Shlwapi.lib user32.lib
    )

elseif(PLATFORM_LINUX)
    # Phase 2: SDL2 entry point + portable support files
    # Phase 3+ will add state.cpp, support.cpp, utility.cpp, wasabi.cpp
    # with OpenGL/GLM equivalents for D3D math
    set(MILKDROP3_SOURCES
        main_linux.cpp
        glcontext.cpp
        glshader.cpp
        milkdrop_renderer.cpp
        fft.cpp
    )

    add_executable(milkdrop3
        ${MILKDROP3_SOURCES}
    )

    target_include_directories(milkdrop3 PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_CURRENT_SOURCE_DIR}/../audio
        ${GLM_INCLUDE_DIRS}
    )

    target_link_libraries(milkdrop3 PRIVATE
        nseel2
        ${OPENGL_LIBRARIES}
        ${SDL2_LIBRARIES}
        ${GLEW_LIBRARIES}
        ${GLM_LIBRARIES}
    )

    if(PIPEWIRE_FOUND)
        target_link_libraries(milkdrop3 PRIVATE ${PIPEWIRE_LIBRARIES})
        target_compile_definitions(milkdrop3 PRIVATE HAVE_PIPEWIRE)
    endif()
endif()

# Compile definitions
target_compile_definitions(milkdrop3 PRIVATE
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<NOT:$<CONFIG:Debug>>:NDEBUG>
)

# Copy resources to build directory
add_custom_command(TARGET milkdrop3 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/../resources
        ${CMAKE_CURRENT_BINARY_DIR}/resources
    COMMENT "Copying resources..."
)
