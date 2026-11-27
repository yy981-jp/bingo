cmake_minimum_required(VERSION 3.20...4.1)
project(bg) # bingo
add_compile_options(-fdiagnostics-color=always)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)

# ソースファイル
file(GLOB SOURCES src/*.cpp)

add_subdirectory(external/CLI11)

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
	${CMAKE_SOURCE_DIR}/external/
	external/CLI11/include
)

# ライブラリをリンク
target_link_libraries(${PROJECT_NAME} PRIVATE
)

# mode
target_compile_options(${PROJECT_NAME} PRIVATE
	$<$<CONFIG:Release>:-O3 -march=native>
	$<$<CONFIG:RelWithDebInfo>:-O3 -march=native -g>
)
