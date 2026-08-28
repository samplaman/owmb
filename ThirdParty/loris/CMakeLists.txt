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

cmake_minimum_required(VERSION 3.22)

project(Loris VERSION 2.1 LANGUAGES C CXX)

# the sources are C++17-clean (no auto_ptr, unary/binary_function,
# bind1st/2nd, or not1); make that a checked guarantee
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#----------------------------------------------------------------
# Version components, substituted into src/loris.h.in
# (keep consistent with the project() version above)
#----------------------------------------------------------------
set(LORIS_MAJOR_VERSION ${PROJECT_VERSION_MAJOR})
set(LORIS_MINOR_VERSION ${PROJECT_VERSION_MINOR})
# must be a number, not empty: it is substituted verbatim into the public
# loris.h, where an empty #define cannot be used in an expression
set(LORIS_SUBMINOR_VERSION "0")
set(LORIS_PRERELEASE_STR "development")
set(LORIS_VERSION_STR "Loris ${PROJECT_VERSION}${LORIS_PRERELEASE_STR}")

#----------------------------------------------------------------
# Options
#----------------------------------------------------------------
option(LORIS_FFTW "Use the FFTW library if available (otherwise use the bundled FFT)" OFF)
option(LORIS_UTILS "Build the command line utilities" OFF)
option(LORIS_PYTHON "Build the SWIG Python module" OFF)
option(LORIS_DEBUG "Enable internal Loris debugging code (not recommended)" OFF)

# Oldest Python the SWIG module is supported on. CI builds against exactly this
# version, so raising the floor is a one-line change here and in ci.yml.
set(LORIS_PYTHON_MIN 3.12)

#----------------------------------------------------------------
# Look for FFTW3. Optional: without it, Loris uses the bundled
# FFT (fftsg.c), and infrequent non-power-of-two DFTs are slower.
#----------------------------------------------------------------
if(LORIS_FFTW)
    find_path(FFTW3_INCLUDE_DIR fftw3.h HINTS /opt/homebrew/include /usr/local/include)
    find_library(FFTW3_LIBRARY fftw3 HINTS /opt/homebrew/lib /usr/local/lib)
    if(FFTW3_INCLUDE_DIR AND FFTW3_LIBRARY)
        set(LORIS_HAVE_FFTW3 TRUE)
    else()
        message(WARNING "Not using the FFTW library. Infrequent non-power-of-two DFTs will be slow.")
    endif()
endif()

add_subdirectory(src)

if(LORIS_UTILS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/utils")
    add_subdirectory(utils)
endif()

#----------------------------------------------------------------
# Python module (SWIG). Python and SWIG are located here at the top
# level so their imported targets are visible in scripting/ and test/.
# SWIG comes from PATH; the interpreter does not — FindPython consults
# virtual environments ahead of PATH, so see the VIRTUAL_ENV note below.
#----------------------------------------------------------------
if(LORIS_PYTHON)
    # Honor an activated virtual environment (VIRTUAL_ENV set): the module is
    # then built for and installed into that venv's site-packages.
    #
    # Validate the path, not merely its presence. FindPython treats both
    # $VIRTUAL_ENV and $CONDA_PREFIX as virtual environments, so a stale
    # VIRTUAL_ENV left behind by a deleted venv would quietly fall through to
    # an activated conda environment and build the module for an unrelated
    # interpreter. Fail loudly instead: the wrong Python is far more expensive
    # to discover at import time than at configure time.
    if(DEFINED ENV{VIRTUAL_ENV} AND NOT "$ENV{VIRTUAL_ENV}" STREQUAL "")
        if(NOT IS_DIRECTORY "$ENV{VIRTUAL_ENV}")
            message(FATAL_ERROR
                "VIRTUAL_ENV is set to '$ENV{VIRTUAL_ENV}', which is not a directory. "
                "Create it (python3 -m venv .venv && .venv/bin/pip install swig), "
                "or unset VIRTUAL_ENV to build against the interpreter on PATH.")
        endif()
        file(REAL_PATH "$ENV{VIRTUAL_ENV}" LORIS_VENV)
        set(Python3_FIND_VIRTUALENV ONLY)
    else()
        # No venv was requested, so take the interpreter from PATH like any
        # other tool. Without this, FindPython's default (FIRST) would prefer
        # an activated conda environment over PATH, making the chosen Python a
        # function of the developer's shell rather than of the preset.
        set(LORIS_VENV "")
        set(Python3_FIND_VIRTUALENV STANDARD)
    endif()
    find_package(Python3 ${LORIS_PYTHON_MIN} COMPONENTS Interpreter Development.Module)
    find_package(SWIG 4.0)
    if(Python3_FOUND AND SWIG_FOUND)
        # ONLY is not on its own enough to guarantee we got the venv that was
        # asked for: a Python3_ROOT_DIR hint in the environment outranks the
        # virtual-environment search, and actions/setup-python exports exactly
        # such a hint. Check the outcome rather than trusting the policy. If the
        # module would not land inside the requested venv then the install is
        # wrong and its RPATH cannot resolve libloris, so stop here.
        if(LORIS_VENV)
            file(REAL_PATH "${Python3_SITEARCH}" _loris_sitearch)
            cmake_path(IS_PREFIX LORIS_VENV "${_loris_sitearch}" NORMALIZE _loris_in_venv)
            if(NOT _loris_in_venv)
                message(FATAL_ERROR
                    "VIRTUAL_ENV asks for '${LORIS_VENV}', but CMake selected "
                    "'${Python3_EXECUTABLE}', whose site-packages is "
                    "'${Python3_SITEARCH}'. Something outranked the virtual "
                    "environment — a Python3_ROOT_DIR hint in the environment will "
                    "do it. Pass -DPython3_EXECUTABLE=${LORIS_VENV}/bin/python3 to "
                    "settle the matter.")
            endif()
        endif()
        add_subdirectory(scripting)
        message(STATUS "Python module: ${Python3_EXECUTABLE} -> ${Python3_SITEARCH}")
        message(STATUS "SWIG: ${SWIG_EXECUTABLE} (${SWIG_VERSION})")
    else()
        message(WARNING
            "Python module cannot be built: need Python >= ${LORIS_PYTHON_MIN} "
            "(found '${Python3_EXECUTABLE}' ${Python3_VERSION}) and SWIG >= 4.0 "
            "(found '${SWIG_EXECUTABLE}' ${SWIG_VERSION}). "
            "Set -DLORIS_PYTHON=OFF to skip it deliberately.")
    endif()
endif()

# include(CTest) defines BUILD_TESTING (default ON)
include(CTest)
if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/test")
    add_subdirectory(test)
endif()

# optional `docs` target, when doxygen is available
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/doc")
    add_subdirectory(doc)
endif()


#----------------------------------------------------------------
# Configuration summary
#----------------------------------------------------------------
if(LORIS_HAVE_FFTW3)
    message(STATUS "FFTW support: Enabled (${FFTW3_LIBRARY})")
else()
    message(STATUS "FFTW support: Disabled, using the bundled FFT")
endif()
message(STATUS "Command line utilities: ${LORIS_UTILS}")
