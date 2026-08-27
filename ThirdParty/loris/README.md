# Loris

[![CI](https://github.com/kellyfitz/loris/actions/workflows/ci.yml/badge.svg)](https://github.com/kellyfitz/loris/actions/workflows/ci.yml)

Loris is an open source C++ class library implementing analysis, manipulation,
and synthesis of digitized sounds using the Reassigned Bandwidth-Enhanced
Additive Sound Model. It includes a C++ class library, a Python module, a
C-linkable interface, and command line utilities.

For more information about Loris and the Reassigned Bandwidth-Enhanced Additive
Model, contact the developers at <loris@cerlsoundgroup.org>, or visit them at
<http://www.cerlsoundgroup.org/Loris/>.

## What is in the box

| Component | Location | Notes |
|---|---|---|
| Core C++ library (`libloris`) | [src/](src/) | Analysis, modification, morphing, synthesis; C++17 |
| Fast block synthesizer | [src/fast-synth-src/](src/fast-synth-src/) | Block-rate additive synthesis, roughly 7–9× faster than the standard engine. Bandwidth enhancement is not yet functional in this path |
| Python module | [scripting/](scripting/) | Generated with SWIG; `import loris` |
| C-linkable interface | [src/loris.h.in](src/loris.h.in) | For callers that cannot use the C++ API; configured into `loris.h` at build time |
| Command line utilities | [utils/](utils/) | `loris-analyze`, `loris-synthesize`, and friends |
| Test suite | [test/](test/) | Run with CTest |
| API documentation | [doc/](doc/) | Doxygen sources and generated documentation |
| Morphing demo | [demo/](demo/) | `morphdemo.py`, a worked end-to-end example |

## Installation

Loris builds with CMake (3.24 or newer) and is built entirely out of tree. For
detailed configuration and installation instructions, see [INSTALL](INSTALL).
Briefly:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build
cmake --install build
```

Installing into a Python virtual environment prefix (`cmake --install build
--prefix /path/to/venv`) puts the `loris` module where that interpreter will
find it.

### Requirements

- A modern, reasonably standard-compliant C++ compiler. Loris requires C++17.
- [SWIG](https://www.swig.org), needed only to build the scripting extensions
  such as the Python module. Also available via `pip install swig`.
- [FFTW](https://www.fftw.org), optional but recommended for best performance.
  FFTW is covered by its own license and copyright, and is entirely separate
  from Loris. Loris builds and runs without it, using a bundled FFT, at the
  cost of slower infrequent non-power-of-two transforms.
- [Doxygen](https://www.doxygen.nl), optional, to regenerate the API
  documentation.

CMake is the only supported build system; the autotools build was removed in
2.0. Every push is built and tested on Linux and macOS in three configurations
(Debug, Release, and without FFTW); see
[.github/workflows/ci.yml](.github/workflows/ci.yml) for the exact packages
each platform needs.

### Build presets

[CMakePresets.json](CMakePresets.json) defines named configurations:

| Preset | Purpose |
|---|---|
| `default`, `release`, `nofftw` | Development builds. These add a project-local `.venv` and Homebrew to `PATH`, so they suit a machine set up that way |
| `ci`, `ci-release`, `ci-nofftw` | The same three configurations with no environment assumptions; these are what CI runs, and what to use on any other machine |

```sh
cmake --preset ci
cmake --build --preset ci
ctest --preset ci
```

Source formatting is defined by [.clang-format](.clang-format) and applied by a
[pre-commit](https://pre-commit.com) hook; see
[.pre-commit-config.yaml](.pre-commit-config.yaml). Run `pre-commit install`
once in a fresh clone to enable it.

## Documentation

For documentation, please see the files in the [doc](doc) subdirectory. With
Doxygen installed, `cmake --build build --target docs` generates the API
documentation.

For a list of major changes to Loris, organized by release number, please see
[NEWS](NEWS).

Maintainers: for the steps to cut a tagged release, see
[RELEASING.md](RELEASING.md).

## Project history

Loris was written by Kelly Fitz and Lippold Haken at the CERL Sound Group, and
has been released since 1999, through version 1.9. Development was hosted on
SourceForge; it moved to GitHub in 2026, and the SourceForge project is no
longer maintained.

Version 2.0 is the result of a revival effort that modernized the build and the
sources without changing the sound model:

| Date | Milestone |
|---|---|
| 2026-07-07 | Full review of the dormant tree; trial build; revival plan |
| 2026-07-08 | Build environment restored; `.gitignore` added |
| 2026-07-09 | Signed-overflow bug fixed in the AIFF IEEE-extended (80-bit) conversions; full test suite passing |
| 2026-07-10 | Migrated from autotools to CMake, with CTest parity and venv-aware Python discovery; autotools, the Csound opcodes and `config.h` removed; verified from a fresh tree |
| 2026-07-12 | VS Code integration; ~525 deprecation warnings eliminated; C++17 pinned; fast block synthesizer absorbed into the library |
| 2026-07-13 | Demo and tests ported off Python 2; obsolete `lorisdoc.py` removed (docstrings now live in the SWIG interface files); version bumped to 2.0; release procedure documented; fast-synth comparison harness added, which immediately found a lookup-table heap overrun |
| 2026-07-14 | Fast-synth phase-offset bug fixed; fast-synth moved into `namespace Loris`; first-generation dead code removed |
| 2026-07-16 | C++ modernization pass; tree-wide clang-format with a pre-commit hook; `.C` sources renamed `.cpp`, preserving history |
| 2026-07-24 | Moved to GitHub; continuous integration on Linux and macOS |

## Copyright and license

Loris is Copyright (c) 1999-2026 by Kelly Fitz and Lippold Haken.

Loris is free software; you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY, without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the file [COPYING](COPYING) or the GNU General Public
License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA.

<loris@cerlsoundgroup.org>
<http://www.cerlsoundgroup.org/Loris/>
