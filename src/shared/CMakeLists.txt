add_library(notiman_shared STATIC)

target_sources(notiman_shared PRIVATE
    config_watcher.h
    config_watcher.cpp
    config.h
    config.cpp
    corner.h
    corner.cpp
    host_ipc.h
    host_ipc.cpp
    hook_parser.h
    hook_parser.cpp
    icon.h
    icon.cpp
    payload.h
    payload.cpp
    positioning.h
    positioning.cpp
    tray_icon.h
    tray_icon.cpp
)

target_link_libraries(notiman_shared PUBLIC third_party)
