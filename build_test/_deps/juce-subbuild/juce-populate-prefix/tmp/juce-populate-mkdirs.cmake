# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/Users/eoinodowd/Desktop/owmb new latest/build_test/_deps/juce-src")
  file(MAKE_DIRECTORY "/Users/eoinodowd/Desktop/owmb new latest/build_test/_deps/juce-src")
endif()
file(MAKE_DIRECTORY
  "/Users/eoinodowd/Desktop/owmb new latest/build_test/_deps/juce-build"
  "/Users/eoinodowd/Desktop/owmb new latest/build_test/_deps/juce-subbuild/juce-populate-prefix"
  "/Users/eoinodowd/Desktop/owmb new latest/build_test/_deps/juce-subbuild/juce-populate-prefix/tmp"
  "/Users/eoinodowd/Desktop/owmb new latest/build_test/_deps/juce-subbuild/juce-populate-prefix/src/juce-populate-stamp"
  "/Users/eoinodowd/Desktop/owmb new latest/build_test/_deps/juce-subbuild/juce-populate-prefix/src"
  "/Users/eoinodowd/Desktop/owmb new latest/build_test/_deps/juce-subbuild/juce-populate-prefix/src/juce-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/Users/eoinodowd/Desktop/owmb new latest/build_test/_deps/juce-subbuild/juce-populate-prefix/src/juce-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/Users/eoinodowd/Desktop/owmb new latest/build_test/_deps/juce-subbuild/juce-populate-prefix/src/juce-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
