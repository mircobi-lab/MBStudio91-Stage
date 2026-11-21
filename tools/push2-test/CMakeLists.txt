cmake_minimum_required(VERSION 3.22)

project(MBStudio91Push2Test VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Le prove del protocollo non hanno bisogno di JUCE: -DPUSH2_ONLY_TESTS=ON
option(PUSH2_ONLY_TESTS "Compila solo le prove del protocollo" OFF)

add_library(push2_protocol STATIC Source/Push2Protocol.cpp)
target_include_directories(push2_protocol PUBLIC Source)

add_executable(Push2ProtocolTests Tests/ProtocolTests.cpp)
target_link_libraries(Push2ProtocolTests PRIVATE push2_protocol)
enable_testing()
add_test(NAME Push2ProtocolTests COMMAND Push2ProtocolTests)

if(PUSH2_ONLY_TESTS)
    return()
endif()

include(FetchContent)

# Stessa versione di JUCE di MBStudio91 Stage
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.15
    GIT_SHALLOW    TRUE)

# libusb per il display (trasferimenti bulk USB), compilata statica dentro l'app
set(LIBUSB_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(LIBUSB_INSTALL_TARGETS OFF CACHE BOOL "" FORCE)
set(LIBUSB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LIBUSB_BUILD_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_Declare(libusb
    GIT_REPOSITORY https://github.com/libusb/libusb-cmake.git
    GIT_TAG        v1.0.30-0
    GIT_SHALLOW    TRUE)

FetchContent_MakeAvailable(JUCE libusb)

juce_add_gui_app(MBStudio91Push2Test
    PRODUCT_NAME "MBStudio91 Push2 Test"
    COMPANY_NAME "MBStudio91"
    BUNDLE_ID com.mbstudio91.push2test
    VERSION ${PROJECT_VERSION}
    MICROPHONE_PERMISSION_ENABLED FALSE
    NEEDS_CURL FALSE
    NEEDS_WEB_BROWSER FALSE)

# Il font e' quello di MBStudio91 Stage (Assets/Fonts nella radice del repository)
# Comfortaa Light/Bold (OFL, file originali di googlefonts/comfortaa) per il nome in stile logo MBStudio
juce_add_binary_data(Push2TestAssets SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../../Assets/Fonts/BebasNeue-Regular.ttf
    Assets/Fonts/Comfortaa-Light.ttf
    Assets/Fonts/Comfortaa-Bold.ttf)

target_sources(MBStudio91Push2Test PRIVATE
    Source/Main.cpp
    Source/MainComponent.cpp
    Source/Push2Display.cpp
    Source/Push2Screen.cpp)

target_compile_definitions(MBStudio91Push2Test PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    PUSH2TEST_VERSION="${PROJECT_VERSION}")

target_link_libraries(MBStudio91Push2Test PRIVATE
    push2_protocol
    Push2TestAssets
    usb-1.0
    juce::juce_audio_devices
    juce::juce_gui_basics
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
