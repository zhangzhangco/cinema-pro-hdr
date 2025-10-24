# OFX Plugin CMakeLists.txt

if(NOT BUILD_OFX)
    return()
endif()

# OFX plugin sources
set(OFX_SOURCES
    cinema_pro_hdr_plugin.cpp
)

# Create OFX plugin
add_library(cinema_pro_hdr_ofx MODULE ${OFX_SOURCES})

# Link libraries
target_link_libraries(cinema_pro_hdr_ofx 
    cinema_pro_hdr_core
    OfxSupport
)

# Set plugin properties
set_target_properties(cinema_pro_hdr_ofx PROPERTIES
    PREFIX ""
    SUFFIX ".ofx"
    BUNDLE TRUE
)

# Platform-specific settings
if(PLATFORM_MACOS)
    set_target_properties(cinema_pro_hdr_ofx PROPERTIES
        BUNDLE_EXTENSION "ofx"
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist
    )
elseif(PLATFORM_LINUX)
    set_target_properties(cinema_pro_hdr_ofx PROPERTIES
        SUFFIX ".ofx.so"
    )
elseif(PLATFORM_WINDOWS)
    set_target_properties(cinema_pro_hdr_ofx PROPERTIES
        SUFFIX ".ofx"
    )
endif()

# Install OFX plugin
if(PLATFORM_MACOS)
    install(TARGETS cinema_pro_hdr_ofx DESTINATION /Library/OFX/Plugins)
elseif(PLATFORM_LINUX)
    install(TARGETS cinema_pro_hdr_ofx DESTINATION /usr/OFX/Plugins)
elseif(PLATFORM_WINDOWS)
    install(TARGETS cinema_pro_hdr_ofx DESTINATION "C:/Program Files/Common Files/OFX/Plugins")
endif()