# D-Fend Reborn

C++17 / Qt 6 port of **D-Fend Reloaded**, the DOSBox frontend, converted
directly from the original Delphi sources (`D-Fend-Reloaded-Base`).

* All 172 forms were converted from the Delphi `.dfm` files into Qt Designer
  `.ui` files (`src/forms`) with the original control positions, captions,
  icons and glyphs; a small VCL layout engine reproduces `Align`/`Anchors`
  behaviour so the dialogs look and resize like the originals.
* Every Delphi unit has a C++ counterpart with the same name
  (`src/core` for non visual units, `src/forms` for forms and frames).
* `src/compat` contains a thin Delphi RTL/VCL compatibility layer.

See [PORTING.md](PORTING.md) for the conventions used by the port.

## Building

Requirements: CMake ≥ 3.19, a C++17 compiler and Qt ≥ 6.4 with the Widgets,
Network, Xml and Multimedia modules.

```
cmake -S . -B build
cmake --build build
```

or open `CMakeLists.txt` in Qt Creator.
