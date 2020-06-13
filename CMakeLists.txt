cmake_minimum_required(VERSION 3.10)

project(86x64 VERSION 0.0)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_package(LIEF)
find_package(xed)

# add_executable(86x64 main.cpp instrs.cpp function.cpp visitor.cpp)

# target_link_libraries(86x64 PRIVATE ${LIEF_LIBRARIES} ${xed_LIBRARIES})
# target_include_directories(86x64 PRIVATE ${LIEF_INCLUDE_DIRS} ${xed_INCLUDE_DIRS})

# add_executable(readmacho readmacho.cpp)
# target_link_libraries(readmacho PRIVATE ${LIEF_LIBRARIES})
# target_include_directories(readmacho PRIVATE ${LIEF_INCLUDE_DIRS})

add_subdirectory(c)
add_subdirectory(c++)
