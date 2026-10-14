cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME stm32-2)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
option(APP_UPGRADE_LAYOUT "Build application for the serial bootloader layout" OFF)
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME})

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Collect user source files
file(GLOB_RECURSE BSP_SOURCES "BSP/Src/*.c")
file(GLOB_RECURSE APP_COMPONENT_SOURCES "Components/*/*.c")
file(GLOB_RECURSE ALGORITHM_SOURCES "Algorithm/*.c")
file(GLOB_RECURSE APPLICATION_SOURCES "Application/*.c")
file(GLOB_RECURSE UTILS_SOURCES "Utils/*.c")
file(GLOB PROTOCOL_SOURCES "Protocol/*.c")

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
    ${BSP_SOURCES}
    ${APP_COMPONENT_SOURCES}
    ${ALGORITHM_SOURCES}
    ${APPLICATION_SOURCES}
    ${UTILS_SOURCES}
    ${PROTOCOL_SOURCES}
    Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_iwdg.c
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
    BSP/Inc
    Components/Display
    Components/Sensors
    Components/Actuators
    Components/Input
    Components/Output
    Algorithm
    Application
    Utils
    Protocol
    Bootloader
)

# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx

    # Add user defined libraries
)

get_filename_component(HOOD_COMPILER_DIR "${CMAKE_C_COMPILER}" DIRECTORY)
find_program(HOOD_OBJCOPY NAMES arm-none-eabi-objcopy HINTS "${HOOD_COMPILER_DIR}" REQUIRED)
target_link_options(${CMAKE_PROJECT_NAME} PRIVATE "-T${CMAKE_SOURCE_DIR}/STM32F411XX_FLASH.ld")
set_property(TARGET ${CMAKE_PROJECT_NAME} APPEND PROPERTY LINK_DEPENDS "${CMAKE_SOURCE_DIR}/STM32F411XX_FLASH.ld")
if(APP_UPGRADE_LAYOUT)
    target_compile_definitions(stm32cubemx INTERFACE APP_UPGRADE_LAYOUT USER_VECT_TAB_ADDRESS VECT_TAB_OFFSET=0x00020000U)
    target_link_options(${CMAKE_PROJECT_NAME} PRIVATE "-Wl,--defsym=APP_FLASH_ORIGIN=0x08020000" "-Wl,--defsym=APP_FLASH_LENGTH=0x1FFE0")
endif()
add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND "${HOOD_OBJCOPY}" -O binary $<TARGET_FILE:${CMAKE_PROJECT_NAME}> ${CMAKE_BINARY_DIR}/stm32-2.bin)

# 独立 Bootloader：不链接应用任务、FreeRTOS 或电机控制代码。
add_executable(bootloader Bootloader/boot_main.c Bootloader/boot_platform.c Bootloader/boot_update.c
    Protocol/serial_protocol.c Utils/crc.c startup_stm32f411xe.s Core/Src/system_stm32f4xx.c
    Core/Src/syscalls.c Core/Src/sysmem.c)
set(BOOT_HAL_DRIVERS hal hal_cortex hal_rcc hal_rcc_ex hal_gpio hal_uart hal_dma hal_dma_ex
    hal_pwr hal_pwr_ex hal_flash hal_flash_ex hal_iwdg)
foreach(driver IN LISTS BOOT_HAL_DRIVERS)
    target_sources(bootloader PRIVATE Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_${driver}.c)
endforeach()
target_include_directories(bootloader PRIVATE Bootloader Protocol Utils Core/Inc
    Drivers/STM32F4xx_HAL_Driver/Inc Drivers/STM32F4xx_HAL_Driver/Inc/Legacy
    Drivers/CMSIS/Include Drivers/CMSIS/Device/ST/STM32F4xx/Include)
target_compile_definitions(bootloader PRIVATE USE_HAL_DRIVER STM32F411xE)
target_link_options(bootloader PRIVATE "-T${CMAKE_SOURCE_DIR}/Bootloader/bootloader.ld"
    "-Wl,-Map=bootloader.map")
set_property(TARGET bootloader APPEND PROPERTY LINK_DEPENDS "${CMAKE_SOURCE_DIR}/Bootloader/bootloader.ld")
add_custom_command(TARGET bootloader POST_BUILD
    COMMAND "${HOOD_OBJCOPY}" -O binary $<TARGET_FILE:bootloader> ${CMAKE_BINARY_DIR}/bootloader.bin)
