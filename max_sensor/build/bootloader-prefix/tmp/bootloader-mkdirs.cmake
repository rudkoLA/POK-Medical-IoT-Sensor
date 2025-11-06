# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/rusik_99/esp-idf/components/bootloader/subproject"
  "/home/rusik_99/UCU/POK/POK-Medical-IoT-Sensor/max_sensor/build/bootloader"
  "/home/rusik_99/UCU/POK/POK-Medical-IoT-Sensor/max_sensor/build/bootloader-prefix"
  "/home/rusik_99/UCU/POK/POK-Medical-IoT-Sensor/max_sensor/build/bootloader-prefix/tmp"
  "/home/rusik_99/UCU/POK/POK-Medical-IoT-Sensor/max_sensor/build/bootloader-prefix/src/bootloader-stamp"
  "/home/rusik_99/UCU/POK/POK-Medical-IoT-Sensor/max_sensor/build/bootloader-prefix/src"
  "/home/rusik_99/UCU/POK/POK-Medical-IoT-Sensor/max_sensor/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/rusik_99/UCU/POK/POK-Medical-IoT-Sensor/max_sensor/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/rusik_99/UCU/POK/POK-Medical-IoT-Sensor/max_sensor/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
