# Windows build architecture

The workflow runs natively on GitHub's Windows 2022 x64 runner using MSYS2 UCRT64,
GCC, CMake and Ninja. GrandOrgue and Steinberg sources are pinned in
`dependencies.json`. GitHub Actions are pinned by commit. MSYS2 packages remain
rolling dependencies; each artifact records their actual versions.

## Bundle

The host-facing `Contents/x86_64-win/OrganVST.vst3` is a small C loader. It forwards
InitDll, GetPluginFactory and ExitDll to OrganVST-engine.dll. It loads that engine
using LoadLibraryEx with the bundle's DLL directory in the search path, rather than
modifying the DAW's global PATH or DLL directory. Loading happens when the host
initializes the module, never from DllMain or the audio callback. The engine DLL
contains the actual processor/controller, GrandOrgue engine and VSTGUI editor.
Both binaries and their runtime libraries must stay together in the bundle.

Dependency staging recursively inspects PE imports and copies MSYS2 runtime DLLs;
Windows system DLLs are not redistributed. Validation tools get their own runtime
copies. The final packaged-plugin check removes MSYS2 from PATH. This is a
packaging check, not proof of compatibility with every DAW or third-party plugin.

## MinGW adaptations

- Omit the SDK's unrelated standalone host applications and VSTGUI scripting.
- Use VSTGUI's existing HWND/Direct2D renderer instead of DirectComposition APIs
  absent from the MinGW headers. Use system fonts; embedded custom fonts are not
  enabled in this Windows build.
- Replace Microsoft PPL with owned worker threads and a message-only UI window.
  Workers join before module teardown. A native CTest checks serial ordering,
  background-to-UI delivery, independent executors and shutdown.
- Use the Windows aligned allocation/free pair in the SDK on MinGW too.
- Use a Unicode validator entry point and explicitly link Windows UI libraries.
- Set the VST3 bundle output path explicitly for the single-config Ninja generator.

All dependency edits are tracked patches. The bootstrap script audits root and
submodule changes, refuses unexpected modifications, and supports repeat runs.

## Build and reproduce

Use the MSYS2 UCRT64 shell and install the packages listed in
`.github/workflows/windows.yml`. For a Git checkout, first run
`python scripts/bootstrap.py`. The corresponding-project-source ZIP already has
the dependency source trees and applied patches; skip bootstrap for that snapshot.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_FLAGS_RELEASE="-O2 -g -DNDEBUG" \
  -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ \
  -DSMTG_CREATE_BUNDLE_FOR_WINDOWS=ON
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
python scripts/package_windows.py --build build --output dist --runtime "$MINGW_PREFIX/bin"
```

Keep the build manifest with diagnostic logs. Debug symbols are DWARF `.debug`
files, and the shipped engine also retains its debug information. A successful
CI run does not mark the FL Studio checklist complete; that remains colleague
host testing. See WINDOWS_TESTING.md for installation and known alpha limitations.
