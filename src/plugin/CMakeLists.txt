juce_add_plugin(ExpressiveComposer
    COMPANY_NAME "Expressive Composer"
    BUNDLE_ID "com.expressivecomposer.plugin"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Ecmp
    PLUGIN_CODE Ec01
    PRODUCT_NAME "Expressive Composer"
    FORMATS Standalone VST3 AU
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
)

target_sources(ExpressiveComposer PRIVATE
    PluginProcessor.cpp
    PluginEditor.cpp
)

target_compile_definitions(ExpressiveComposer PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(ExpressiveComposer PRIVATE
    ec_render
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_gui_extra
)

if(MSVC AND TARGET ExpressiveComposer_Standalone)
    target_link_options(ExpressiveComposer_Standalone PRIVATE "/MAP:${CMAKE_BINARY_DIR}/ec_standalone.map" "/DEBUG")
endif()

if(EC_BUILD_CLAP AND TARGET clap_juce_extensions_plugin)
    clap_juce_extensions_plugin(TARGET ExpressiveComposer
        CLAP_ID "com.expressivecomposer.plugin"
        CLAP_FEATURES instrument synthesizer
    )
endif()
