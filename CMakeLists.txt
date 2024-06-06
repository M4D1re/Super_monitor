cmake_minimum_required(VERSION 3.10)
project(CPU_monitor_optimization_)

set(CMAKE_C_STANDARD 11)

add_executable(CPU_monitor_optimization_ main.c)

# Link against the necessary libraries
target_link_libraries(CPU_monitor_optimization_ kernel32 user32 gdi32 winspool shell32 ole32 oleaut32 uuid comdlg32 advapi32 pdh)
