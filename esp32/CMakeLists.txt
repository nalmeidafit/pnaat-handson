cmake_minimum_required(VERSION 3.16)

# Este projeto é fixo para ESP32-S3.
set(IDF_TARGET "esp32s3" CACHE STRING "ESP-IDF target" FORCE)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(pnaat_esteira)
