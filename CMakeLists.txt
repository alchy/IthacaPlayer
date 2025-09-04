cmake_minimum_required(VERSION 3.22)

project(ITHACA_PLAYER VERSION 0.0.1)

add_subdirectory(JUCE)

juce_add_plugin(IthacaPlayer
    COMPANY_NAME "Lord Audio"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    PLUGIN_MANUFACTURER_CODE Lau0
    PLUGIN_CODE Itca
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "IthacaPlayer")

juce_generate_juce_header(IthacaPlayer)

target_sources(IthacaPlayer
    PRIVATE
        # Core system
        Logger.h
        Logger.cpp
        
        # Enhanced sample management with dynamic levels
        SampleLoader.h
        SampleLoader.cpp
        SampleLibrary.h
        SampleLibrary.cpp
        
        # MIDI and voice management
        MidiStateManager.h
        MidiStateManager.cpp
        VoiceManager.h
        VoiceManager.cpp
        
        # JUCE plugin interface
        PluginEditor.h
        PluginEditor.cpp
        PluginProcessor.h
        PluginProcessor.cpp)

target_compile_definitions(IthacaPlayer
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(IthacaPlayer
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_formats  # Přidáno pro WAV loading
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)