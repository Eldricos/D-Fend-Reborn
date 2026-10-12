# Brief for porting a batch of units

You port part of the Delphi program **D-Fend Reloaded** to C++17 / Qt 6.

* Delphi sources (read only): `/home/user/D-Fend-Reloaded-Base/<Unit>.pas` and `<Unit>.dfm`
* C++ project: `/home/user/d-fend-reborn`
* **Read `/home/user/d-fend-reborn/PORTING.md` first** – it defines all conventions.
  Then read the compat headers you need (`src/compat/*.h`); they are the API you build on.

For every unit of your batch:

1. A header `src/core/<Unit>.h` (non visual units) or `src/forms/<Unit>.h`
   (forms/frames, together with the generated `<Unit>.ui`) already exists. It was
   generated mechanically from the Pascal interface section. Review it, fix what
   the generator got wrong, add what C++ needs (e.g. `showEvent` override,
   `_Initialization`/`_Finalization` functions, private helpers) – but keep the
   public API stable, other units are ported in parallel against it.
2. Write `<Unit>.cpp` next to the header translating the **complete**
   implementation section, routine by routine, as literally as possible. No stubs,
   no "simplified" versions, no omitted features. Use `// TODO(port): …` only for
   things that truly have no equivalent (e.g. a Windows API on other platforms) and
   still provide a working fallback (`#ifdef Q_OS_WIN` + portable code).
3. Forms: the `.ui` contains all DFM components (`ui.<Name>`). Wire every DFM event
   listed in the header comment block, port FormCreate/FormShow/etc. as described in
   PORTING.md. Non-visual DFM components (TOpenDialog, TSaveDialog, TTimer,
   TTrayIcon, TApplicationEvents, TImageList) are not in the `.ui` – create the
   equivalents in code using the properties from the `.dfm`.

Rules:

* Only create/modify files of **your** units (`<Unit>.h`, `<Unit>.cpp`). Do not edit
  `src/compat/*`, any `.ui` file, other units' headers, `CMakeLists.txt` or
  `src/main.cpp`. If something there must change, put the exact proposal into your
  final report. (If another unit's header is wrong in a way that blocks you,
  work around it locally in your `.cpp` and report it.)
* No git commands that change state (no commit, checkout, stash, reset).
* Verification (both must report zero errors before you finish):
  * `cd /home/user/d-fend-reborn && tools/check_headers.sh src/<dir>/<Unit>.h …` (only your headers)
  * `tools/check_source.sh src/<dir>/<Unit>.cpp …`
* Be economical: read each Pascal file once in big chunks, don't dump generated
  headers repeatedly, don't re-read files you just wrote.

Final report (concise, at most ~400 words): files written; anything not ported
faithfully; changes to your public headers that other units must know about;
requested changes to compat / other units (exact code).
