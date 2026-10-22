cmake_minimum_required(VERSION 3.25)
project(FormulaT CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_compile_options(-Wall -Wextra)

option(TSAN "Build with ThreadSanitizer" OFF)
if(TSAN)
  add_compile_options(-fsanitize=thread)
  add_link_options(-fsanitize=thread)
endif()

find_package(Boost REQUIRED)
find_package(Threads REQUIRED)
find_package(GTest REQUIRED)

# Core library: everything except the GUI. Tests link this, never SFML.
add_library(formulat STATIC
  src/polynomial.cpp
  src/judge.cpp
  src/engines.cpp
  src/binary.cpp
  src/car.cpp
  src/qualifying.cpp
  src/runsection.cpp
  src/redflag.cpp
  src/race.cpp
  src/sequential.cpp
  src/serialize.cpp
  src/timingtower.cpp
  src/track.cpp
)
target_include_directories(formulat PUBLIC include)
target_link_libraries(formulat PUBLIC Boost::headers Threads::Threads)

# The programs students run (from the repo root: they read tracks/, write results/).
add_executable(qualifying apps/qualifying.cpp)
target_link_libraries(qualifying PRIVATE formulat)
add_executable(race apps/race.cpp)
target_link_libraries(race PRIVATE formulat)

enable_testing()

# The given code (domain model, referee, engines). Passes from day one.
add_executable(test_given tests/given_test.cpp)
target_link_libraries(test_given PRIVATE formulat GTest::gtest_main)
target_compile_definitions(test_given PRIVATE FORMULAT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
add_test(NAME given COMMAND test_given)

# One test binary per clase, so a half-written clase can't break the others.
# (A student repo may not have the later clases' tests yet.)
foreach(n 1 2 3 4 5)
  if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/clase${n}_test.cpp)
    continue()
  endif()
  add_executable(test_clase${n} tests/clase${n}_test.cpp)
  target_link_libraries(test_clase${n} PRIVATE formulat GTest::gtest_main)
  # Tests find the shipped files (tracks/) wherever they are run from.
  target_compile_definitions(test_clase${n} PRIVATE FORMULAT_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
  add_test(NAME clase${n} COMMAND test_clase${n})
endforeach()

# Reference-only tools (the skeleton drops tools/).
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tools/calibrate)
  add_executable(calibrate tools/calibrate/calibrate.cpp)
  target_link_libraries(calibrate PRIVATE formulat)
endif()
