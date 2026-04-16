cmake_minimum_required(VERSION 3.26)
project(garraiobide LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Qt configuration
set(CMAKE_AUTOMOC ON)     # Enable the Meta-Object Compiler (for signals/slots)
set(CMAKE_AUTOUIC ON)     # Enable the User Interface Compiler (for .ui files)
set(CMAKE_AUTORCC ON)     # Enable the Resource Compiler (for .qrc files)

include(FetchContent)
FetchContent_Declare(
        QGeoView
        GIT_REPOSITORY https://github.com/AmonRaNet/QGeoView.git
        GIT_TAG        1.1.0
)
FetchContent_MakeAvailable(QGeoView)

enable_testing()

add_subdirectory(src)
add_subdirectory(tests)