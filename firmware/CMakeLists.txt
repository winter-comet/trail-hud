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
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif ()

# Set the project name
set(CMAKE_PROJECT_NAME firmware)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME})
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

# User-defined sources
set(USER_SRC
        Core/Src/hm10.c
        Core/Src/hm10_packet.c
        Core/Src/mpu6050.c
        Core/Src/mpu6050_packet.c
        Core/Src/debug_terminal.c
)

# BSP sources
set(BSP_SRC
        Drivers/BSP/STM32H750B-DK/stm32h750b_discovery_bus.c
        Drivers/BSP/STM32H750B-DK/stm32h750b_discovery_lcd.c
        Drivers/BSP/STM32H750B-DK/stm32h750b_discovery_sdram.c
        Drivers/BSP/STM32H750B-DK/stm32h750b_discovery_ts.c

        Drivers/BSP/Components/ft5336/ft5336.c
        Drivers/BSP/Components/ft5336/ft5336_reg.c
        Drivers/BSP/Components/mt48lc4m32b2/mt48lc4m32b2.c
)

# HAL sources required by LCD/SDRAM BSP
set(HAL_SRC
        Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_ltdc.c
        Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_ltdc_ex.c
        Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_dma2d.c
        Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_sdram.c
        Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_ll_fmc.c
)

# Utility sources
set(UTILITIES_SRC
        Utilities/lcd/stm32_lcd.c
)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE
        ${USER_SRC}
        ${BSP_SRC}
        ${HAL_SRC}
        ${UTILITIES_SRC}
)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
        Core/Inc
        Drivers/STM32H7xx_HAL_Driver/Inc
        Drivers/BSP/STM32H750B-DK
        Drivers/BSP/Components/ft5336
        Drivers/BSP/Components/mt48lc4m32b2
        Drivers/BSP/Components/Common
        Utilities/lcd
        Utilities/Fonts
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
