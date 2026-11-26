cmake_minimum_required(VERSION 3.20)
project(instagram_clone_backend LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_EXTENSIONS OFF)

if (MSVC)
  add_compile_options(/W3 /bigobj /utf-8)
else()
  add_compile_options(-Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers)
endif()

set(ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(MYSQL_ROOT "C:/Program Files/MySQL/MySQL Server 8.0")

include_directories(
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/vendor/crow"
  "${CMAKE_CURRENT_SOURCE_DIR}/vendor/asio"
  "${CMAKE_CURRENT_SOURCE_DIR}/vendor"
  "${CMAKE_CURRENT_SOURCE_DIR}/vendor/nlohmann"
  "${CMAKE_CURRENT_SOURCE_DIR}/vendor/argon2"
  "${MYSQL_ROOT}/include"
)

# ------------------------------------------------------------------ DSA library
file(GLOB DSA_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/dsa/*.cpp")
add_library(ig_dsa STATIC ${DSA_SOURCES})
target_include_directories(ig_dsa PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

# ------------------------------------------------------------------ Argon2
add_library(ig_argon2 STATIC
  "${CMAKE_CURRENT_SOURCE_DIR}/vendor/argon2/argon2.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/vendor/argon2/core.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/vendor/argon2/encoding.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/vendor/argon2/ref.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/vendor/argon2/thread.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/vendor/argon2/blake2/blake2b.c"
)
target_include_directories(ig_argon2 PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/vendor/argon2")

# ------------------------------------------------------------------ Backend core
file(GLOB CORE_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/src/core/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/database/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/services/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/controllers/*.cpp")
add_library(ig_backend STATIC ${CORE_SOURCES})
target_link_libraries(ig_backend PUBLIC ig_dsa ig_argon2)

# ------------------------------------------------------------------ Server
add_executable(ig_server "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")
target_link_libraries(ig_server PRIVATE ig_backend ig_dsa ig_argon2
  "${MYSQL_ROOT}/lib/libmysql.lib"
  ws2_32 wsock32)

if (WIN32)
  add_custom_command(TARGET ig_server POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${MYSQL_ROOT}/lib/libmysql.dll" "$<TARGET_FILE_DIR:ig_server>")
endif()

# ------------------------------------------------------------------ Tests
add_executable(ig_dsa_tests "${ROOT_DIR}/tests/test_dsa.cpp")
target_link_libraries(ig_dsa_tests PRIVATE ig_dsa)

enable_testing()
add_test(NAME dsa_unit_tests COMMAND ig_dsa_tests)
