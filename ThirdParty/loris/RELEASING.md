# Releasing Loris

How to cut a tagged Loris release. Written for the 2.0 release; the steps
generalize to later ones. Work on `master` (or a `release-X.Y` branch if you
prefer to stabilize separately).

The single source of truth for the version is the `project()` call in the
top-level `CMakeLists.txt`; everything else derives from it.


## 1. Pre-release verification

All of these must pass on a clean tree before tagging.

```sh
# clean, warning-free build (Debug and Release)
rm -rf build && cmake --preset default   && cmake --build --preset default   -j8
rm -rf build-release && cmake --preset release && cmake --build --preset release -j8
#   -> both must build with zero warnings

# full test suite
ctest --preset default            # must be 14/14

# the bundled-FFT configuration also builds and analyzes
cmake --preset nofftw && cmake --build --preset nofftw -j8
ctest --preset nofftw             # 14/14

# the Python module installs and imports from anywhere
cmake --install build --prefix .venv
( cd /tmp && ".../.venv/bin/python3" -c "import loris; print(loris.version())" )

# the morphing demo runs end-to-end
( cd demo && python3 morphdemo.py )     # 16 output files, no errors

# fresh-tree sanity: nothing depends on stale generated files
#   rsync the tree (minus .git/build*/.venv) to an empty dir, then
#   cmake -S <copy> -B <copy>/build && cmake --build && ctest  -> 14/14
```

Also confirm `import loris; help(loris)` shows the expected docstrings (they
come from the SWIG `.i` files).


## 2. Finalize the version number

Two edits, both in the build files. The marketing version and the library
ABI version (SOVERSION) are independent — decide each.

**a. Drop the prerelease suffix** so `loris.version()` reads a clean
`Loris 2.0` instead of `Loris 2.0development`. In `CMakeLists.txt`:

```cmake
set(LORIS_PRERELEASE_STR "")      # was "development"
```

To change the number itself for a future release, edit only the
`project(Loris VERSION X.Y …)` line — the `LORIS_*_VERSION` macros and
`loris.version()` follow automatically. (`LORIS_SUBMINOR_VERSION` is a
separate hand-set field, currently empty; set it for an X.Y.Z release.)

**b. Consider bumping the library SOVERSION.** In `src/CMakeLists.txt`:

```cmake
set_target_properties(loris PROPERTIES VERSION 13.0.0 SOVERSION 13)
```

This is the binary-compatibility version (the old libtool `-version-info`
lineage), NOT the marketing version. The 2.0 changes (C++17, `unique_ptr`
in public headers) broke ABI, so bumping to `VERSION 14.0.0 SOVERSION 14`
is defensible for 2.0. It only matters if something external links
`libloris` by version; if in doubt, bump it — a wrongly-shared soname is
worse than a spurious one.

Reconfigure and confirm:

```sh
cmake --preset default && cmake --build --preset default -j8
.venv/bin/python3 -c "import loris; print(loris.version())"   # -> Loris 2.0
```


## 3. Finalize NEWS

The `changes since 1.9 release:` section in `NEWS` already lists the 2.0
changes. Before tagging: reread it, retitle it to `Loris 2.0 (YYYY-MM-DD):`
with the release date, and update the intro paragraph at the top of the
file if needed.


## 4. Commit and tag

```sh
git add CMakeLists.txt src/CMakeLists.txt NEWS
git commit -m "Release Loris 2.0"

# this repo has no version-tag convention yet (only SVN-to-Git-conversion);
# establishing `vX.Y` here:
git tag -a v2.0 -m "Loris 2.0"

git push origin master
git push origin v2.0
```

`origin` is GitHub (`git@github.com:kellyfitz/loris.git`). Pushing the tag
makes it available on the repository's Releases page; attach the source
tarball there, which is what the SourceForge file area used to provide.


## 5. Post-release housekeeping

- Bump `master` back to a development version so it does not claim to be
  the released 2.0: raise `project(... VERSION 2.1)` (or `2.0.1`) and set
  `LORIS_PRERELEASE_STR "development"` again. Commit as "Begin 2.1
  development".
- Delete merged feature branches: `feature/fast-synthesis` was absorbed
  into master (`git branch -d feature/fast-synthesis` and, if it exists on
  the remote, `git push origin --delete feature/fast-synthesis`).


## 6. Optional: publish a Python wheel

Not required for a source release. If you want `pip install loris` /
a PyPI upload, add **scikit-build-core** as the PEP 517 build backend (it
wraps this CMake build) and configure it to read the version from the
CMake `project()` call, keeping the single source of truth. Then
`python -m build` produces the wheel. Do NOT use Poetry: its build backend
cannot compile the SWIG/CMake C-extension.
