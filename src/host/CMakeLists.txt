add_executable(notiman-host WIN32
    main.cpp
    toast_window.h
    toast_window.cpp
    toast_manager.h
    toast_manager.cpp
    render_utils.cpp
    acrylic.h
    acrylic.cpp
)

target_link_libraries(notiman-host PRIVATE notiman_shared third_party)

# System libraries
target_link_libraries(notiman-host PRIVATE d2d1 dwrite dwmapi ws2_32 shell32)
