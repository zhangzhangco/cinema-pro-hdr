# Command Line Tools CMakeLists.txt

message(STATUS "Building command line tools")

# Error Handler Demo Program
add_executable(error_handler_demo error_handler_demo.cpp)
target_link_libraries(error_handler_demo cinema_pro_hdr_core)
target_include_directories(error_handler_demo PRIVATE ${CMAKE_SOURCE_DIR}/include)

# Install demo program
install(TARGETS error_handler_demo DESTINATION bin)

# Placeholder for future command line tools
# add_executable(cph_lut_baker ${BAKER_SOURCES})
# add_executable(cph_json_check ${VALIDATOR_SOURCES})
# target_link_libraries(cph_lut_baker cinema_pro_hdr_core)
# target_link_libraries(cph_json_check cinema_pro_hdr_core)