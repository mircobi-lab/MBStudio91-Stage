cmake_minimum_required(VERSION 3.22)

project(MBStudio91Stage VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
# Per compilare con una copia locale di JUCE: -DFETCHCONTENT_SOURCE_DIR_JUCE=/percorso/JUCE
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.15
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

# Sorgenti del motore, condivisi da app e test.
# (I moduli JUCE sono librerie INTERFACE: compilarli in una libreria statica
#  e poi di nuovo nell'app duplicherebbe i simboli, quindi si condivide la lista.)
set(STAGE_CORE_SOURCES
    Source/settings/AppPaths.cpp
    Source/engine/NoteTracker.cpp
    Source/engine/MidiPanic.cpp
    Source/engine/Station.cpp
    Source/plugins/PluginCatalog.cpp
    Source/plugins/ScannerProcess.cpp
)

set(STAGE_DEFINITIONS
    JUCE_PLUGINHOST_VST3=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    STAGE_VERSION="${PROJECT_VERSION}"
    JUCE_STRICT_REFCOUNTEDPOINTER=1
)
if(APPLE)
    list(APPEND STAGE_DEFINITIONS JUCE_PLUGINHOST_AU=1)
endif()

set(STAGE_MODULES
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_gui_extra
    juce::juce_dsp
)

# ---------------------------------------------------------------- app
juce_add_gui_app(MBStudio91Stage
    PRODUCT_NAME "MBStudio91 Stage"
    COMPANY_NAME "MBStudio91"
    BUNDLE_ID com.mbstudio91.stage
    VERSION ${PROJECT_VERSION}
    MICROPHONE_PERMISSION_ENABLED FALSE
    NEEDS_CURL FALSE
    NEEDS_WEB_BROWSER FALSE)

target_sources(MBStudio91Stage PRIVATE
    Source/Main.cpp
    ${STAGE_CORE_SOURCES})

target_include_directories(MBStudio91Stage PRIVATE Source)
target_compile_definitions(MBStudio91Stage PRIVATE ${STAGE_DEFINITIONS})
target_link_libraries(MBStudio91Stage PRIVATE
    ${STAGE_MODULES}
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------- test
file(GLOB STAGE_TEST_SOURCES CONFIGURE_DEPENDS Tests/*.cpp)
juce_add_console_app(StageTests PRODUCT_NAME "StageTests")

target_sources(StageTests PRIVATE
    ${STAGE_TEST_SOURCES}
    ${STAGE_CORE_SOURCES})

target_include_directories(StageTests PRIVATE Source Tests)
target_compile_definitions(StageTests PRIVATE ${STAGE_DEFINITIONS} JUCE_MODAL_LOOPS_PERMITTED=1)
target_link_libraries(StageTests PRIVATE
    ${STAGE_MODULES}
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

enable_testing()
add_test(NAME StageTests COMMAND StageTests)
