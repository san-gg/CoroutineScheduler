# CMakeList.txt : CMake project for CoroutineScheduler, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

if (MSVC)
	# Enable Hot Reload for MSVC compilers if supported.
	if (POLICY CMP0141)
	  cmake_policy(SET CMP0141 NEW)
	  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
	endif()
endif()

project ("CoroutineScheduler")

# Add source to this project's executable.
add_executable (CoroutineScheduler "CoroutineScheduler.cpp" "CoroutineScheduler.hpp" "Fiber/fiber.h" "Fiber/fiber_win_amd64.h" "Fiber/fiber_linux_amd64.h" "Syscalls.cpp"  "includes/Coroutine.h" "Coroutine.cpp")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET CoroutineScheduler PROPERTY CXX_STANDARD 20)
  set_property(TARGET CoroutineScheduler PROPERTY CMAKE_CXX_STANDARD_REQUIRED ON)
  set_property(TARGET CoroutineScheduler PROPERTY CMAKE_CXX_EXTENSIONS OFF)
endif()

# TODO: Add tests and install targets if needed.
