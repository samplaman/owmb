# Loris is Copyright (c) 1999-2026 by Kelly Fitz and Lippold Haken
# <loris@cerlsoundgroup.org>
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

# loris.h is generated from loris.h.in, substituting the version macros
configure_file(loris.h.in ${CMAKE_CURRENT_BINARY_DIR}/loris.h @ONLY)

# source code for Loris classes
set(CPP_SRC
    AiffData.cpp
    AiffFile.cpp
    Analyzer.cpp
    AssociateBandwidth.cpp
    BigEndian.cpp
    Breakpoint.cpp
    BreakpointUtils.cpp
    Channelizer.cpp
    Collator.cpp
    Dilator.cpp
    Distiller.cpp
    Envelope.cpp
    F0Estimate.cpp
    LorisExceptions.cpp
    Filter.cpp
    FourierTransform.cpp
    FrequencyReference.cpp
    Fundamental.cpp
    Harmonifier.cpp
    ImportLemur.cpp
    KaiserWindow.cpp
    LinearEnvelope.cpp
    Marker.cpp
    Morpher.cpp
    NoiseGenerator.cpp
    Notifier.cpp
    Oscillator.cpp
    Partial.cpp
    PartialBuilder.cpp
    PartialList.cpp
    PartialUtils.cpp
    phasefix.cpp
    ReassignedSpectrum.cpp
    Resampler.cpp
    SdifFile.cpp
    Sieve.cpp
    SpcFile.cpp
    SpectralPeakSelector.cpp
    SpectralSurface.cpp
    Synthesizer.cpp
    fftsg.c
)

# source code for the procedural (C) interface
set(PI_SRC
    lorisAnalyzer_pi.cpp
    lorisBpEnvelope_pi.cpp
    lorisException_pi.cpp
    lorisNonObj_pi.cpp
    lorisPartialList_pi.cpp
    lorisUtilities_pi.cpp
)

# fast block synthesizer (work in progress; headers not installed)
set(FASTSYNTH_SRC
    fast-synth-src/BlockOscillator.cpp
    fast-synth-src/BlockSynthBwe.cpp
    fast-synth-src/BlockSynthReader.cpp
    fast-synth-src/r250.c
    fast-synth-src/randlcg.c
)

add_library(loris STATIC ${CPP_SRC} ${PI_SRC} ${FASTSYNTH_SRC})

# Fastsynth_Float_Type is part of the fast-synth API types, so every
# consumer must agree with the library: make the definition PUBLIC.
# (Renders in double precision, as the original Xcode target did.)
target_compile_definitions(loris PUBLIC FASTSYNTH_FLOAT_TYPE=double)

# The libtool -version-info 13:0:0 lineage ended with 1.9. Loris 2.0 breaks
# binary compatibility -- it requires C++17 and puts unique_ptr in public
# headers -- so the soname advances to 14 rather than letting a 1.9-era
# binary load this library.
set_target_properties(loris PROPERTIES VERSION 14.0.0 SOVERSION 14)

# the binary dir (generated loris.h) must precede the source dir, so that
# a stale in-tree loris.h left by an autotools build cannot shadow it
target_include_directories(loris PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/fast-synth-src>
)

target_compile_options(loris PRIVATE -Wno-comment)

if(LORIS_HAVE_FFTW3)
    target_compile_definitions(loris PRIVATE HAVE_FFTW3_H=1)
    target_include_directories(loris PRIVATE ${FFTW3_INCLUDE_DIR})
    target_link_libraries(loris PRIVATE ${FFTW3_LIBRARY})
endif()

if(LORIS_DEBUG)
    target_compile_definitions(loris PRIVATE Debug_Loris)
endif()

if(NOT APPLE)
    target_link_libraries(loris PRIVATE m)
endif()

# installed Loris header files
set(LORIS_HEADERS
    AiffFile.h
    Analyzer.h
    BreakpointEnvelope.h
    Breakpoint.h
    BreakpointUtils.h
    Channelizer.h
    Collator.h
    Dilator.h
    Distiller.h
    Envelope.h
    Exception.h
    F0Estimate.h
    Filter.h
    FourierTransform.h
    FrequencyReference.h
    Fundamental.h
    Harmonifier.h
    ImportLemur.h
    KaiserWindow.h
    LinearEnvelope.h
    LorisExceptions.h
    Marker.h
    Morpher.h
    NoiseGenerator.h
    Notifier.h
    Oscillator.h
    Partial.h
    PartialList.h
    PartialPtrs.h
    PartialUtils.h
    PtrCopyOnWrite.h
    ReassignedSpectrum.h
    Resampler.h
    SdifFile.h
    Sieve.h
    SpcFile.h
    SpectralSurface.h
    Synthesizer.h
)

include(GNUInstallDirs)
install(TARGETS loris LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/loris.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES ${LORIS_HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/loris)
