# dbxl debugger: design

**dbxl** is a Linux clone of IBM's AIX `xldb` 1.2.1.0 graphical debugger: C, plain Xlib, a
plain `Makefile`, and GDB/MI underneath.

**Source of truth:** the real xldb, observed on AIX 4.3.3 and recorded in
`../recon/xldb-observed.md` (passes 1-6) with screenshots in `../recon/screens/`. Where that
document and the older reverse-engineered specs (`xldb-ui-reconstruction-spec.md`,
`xldb-visual-archaeology.md`) or IBM's own help text disagree, **the observed behaviour wins**.
Section references like *[obs p3]* point at the pass in that document.

## 1. Goals and non-goals

**Goals**
1. Pixel-faithful reproduction of xldb's UI at its default settings on a 24-bit TrueColor X
   server with the `8x13` font: layout, colours, glyphs, menus, dialogs, gestures and text formats.
2. xldb's interaction model: one frame, self-managed child panes, chips, pointer-warping
   "cursor", left-click on objects, right-drag to move/resize, autoraise.
3. xldb's configuration surface: its command-line options and `$HOME/.Xdefaults` resources
   (`dbxl.*` instead of `xldb.*`), including `commandList`, geometries, `tagWindows`, `-bw`/`-wb`.
4. A real debugger for Linux C programs (C++ later) via GDB/MI, behind a backend interface
   that LLDB can implement later.

**Non-goals (for now)**
- COBOL, PL/I, Fortran, CICS, Japanese message catalogs.
- xldb's defects: the SIGCONT stop on first Continue, repaint leftovers after dialogs, the
  invisible arrow under `-wb` (see §11 for each).
- Any toolkit dependency (no Xt, Motif, GTK, Xft, Cairo). Only libX11, plus libXext/libXfixes
  if a feature needs them.

**Naming:** every user-visible string says "dbxl", never "xldb": window title (`dbxl <prog>`),
help text, messages (`Exit from dbxl?`), `WM_CLASS "dbxl","Dbxl"`, resource name `dbxl`,
`$HOME/dbxl.help.text`.

## 2. Architecture

```
            +--------------------------------------------------------------+
            |                         dbxl process                         |
            |                                                              |
 X server <-+-> xtk/     toolkit: frame, panes, chips, menus, dialogs,     |
  (Xlib)    |            scrollbars, input strips, glyphs, pointer, loop    |
            |      ^                                                       |
            |      | draw / events                                         |
            |   panes/   Source, Locals, Globals, Monitor, Callers, ...    |
            |      ^                                                       |
            |      | reads state, issues commands                          |
            |   core/    session state, value tree + formatter,          |
            |            breakpoints, layout, resources, command list      |
            |      ^                                                       |
            |      | struct dbg_backend (commands) / dbg_event (results)   |
            |   backend/ gdbmi/  (spawn gdb --interpreter=mi3, MI parser) |
            |            lldb/   (future)                                  |
            +------+-------------------------------------------------------+
                   | pipes
                gdb 17 --interpreter=mi3  --->  inferior
```

- **Single thread, one event loop** (`xtk/loop.c`): `poll()` on the X connection fd and the
  backend's fd(s). X events are drained with `XPending`/`XNextEvent`. Backend output is parsed
  into `dbg_event`s and dispatched to `core`, which marks panes dirty. Panes repaint once per
  loop iteration (coalesced), never from inside a backend callback.
- **Layers only call downward.** `xtk` knows nothing about debugging. `backend` knows nothing
  about X. `core` owns all debugger state.
- **Asynchronous backend:** every command returns immediately with a token. Results arrive as
  events. While the inferior runs, the UI stays live (the busy cursor goes on the frame).

## 3. Source layout

```
dbxl-debugger/
  Makefile                  plain make; `make`, `make test`, `make visual`
  README.md
  docs/DESIGN.md            this file
  src/
    main.c                  argv, resources, window creation, loop
    opts.c   opts.h         command-line options (-a -bg -bw -c -co -display -e -E -F -fg
                            -font -geometry -h -I -i -k -n -name -q -r -title -v -wb)
    resources.c             $HOME/.Xdefaults reader, name selection (-name > $RESOURCE_NAME
                            > basename(argv[0])), typed getters, defaults table
    xtk/
      xtk.h                 public toolkit API
      display.c             open display, visual, colormap, GCs
      color.c               colour roles (§5.1), -bw/-wb schemes
      font.c                font load with fallback message (default "8x13")
      frame.c               top-level window, stipple background, WM hints
      pane.c                pane windows: border, title bar, tags, content area, raise/lower,
                            autoraise, move/resize rubber band, maximize/restore/minimize
      chip.c                minimized-window chips (icon geometries, no re-flow)
      scrollbar.c           9px bars, thumb, arrows, paging, drag
      cellmenu.c            "Window Control" style menus (bordered cells, sticky default)
      listmenu.c            variable-menu style list boxes (section headers, fixed spot)
      dialog.c              yes/no, proceed/cancel, full-width and compact dialogs
      lineedit.c            text field + inline input strip (`:`/`/`), red cursor box
      rubber.c              XOR 0xA5A5A5 outlines
      glyphs.c glyphs.h     arrow, stop sign, disabled stop sign, pointer, bug icon bitmaps
      pointer.c             custom pointer, busy pointer, cell-snapped warping
      loop.c                event loop, timers, dirty-pane repaint
    panes/
      textpane.c            shared line-list pane (scrolling, hit-testing, save-to-file)
      source.c  disasm.c    source/disassembly views (margin glyphs, cyan frame line)
      valuepane.c           shared by Locals, Globals, Monitor (value tree rendering, wrap)
      locals.c globals.c monitor.c
      callers.c commands.c breakpoints.c files.c subprograms.c
      registers.c storage.c threads.c messages.c help.c
    core/
      session.c session.h   program, process state, current/selected frame, stop reasons
      value.c value.h       value tree: kind, type name, detail level, style, children
      format.c              xldb text formats (§6)
      breakpoints.c         breakpoint table, deferred breakpoints, save/load file
      commandlist.c         commandList parser (+, Name=action:key, null, sub())
      layout.c              default geometries, per-window resources, Save layout
      search.c              `:n`, `/x`, `\x`, `?x` over a text pane
      helptext.c            loads help/dbxl.help, sections, links, history
    backend/
      backend.h             interface (§7)
      gdbmi/
        gdbmi.c             spawn gdb, command queue, token matching, event translation
        mi_parse.c mi.h     GDB/MI output grammar -> tree (results, tuples, lists)
        dbxl.py             optional gdb Python helper (§7.3)
  help/
    dbxl.help               dbxl's own help text (§10)
  tests/
    unit/                   mi_parse, format, commandlist, resources, search
    progs/                  C test programs (ports of recon test.c / rich.c, plus threads,
                            signals, deep stacks, long files)
    visual/                 Xvfb harness, scripted interactions, reference PNGs
  tools/
    xdiff.py                pixel diff / crop / zoom helpers (from recon/tools)
```

C11, `-Wall -Wextra -Wpedantic -Werror` in CI builds, no global mutable state outside
`core/session.c` and `xtk/display.c`. Public symbols are prefixed `xtk_`, `pane_`, `dbxl_`
(core) and `dbg_` (backend).

## 4. Window model (xtk)

Observed in *[obs p1, p3, p5]*; this is the part the specs got most wrong.

- **Frame:** one top-level `InputOutput` window, default `957x846+33+73`. `WM_NAME "dbxl <prog>"`
  (or `-title`), `WM_ICON_NAME "dbxl"`, USPosition/USSize, min size 64x64, icon pixmap = bug
  icon. Background = the 4x4 stipple tile (white/`#4876ff`):
  ```
  W b W b
  b W b b
  W b W b
  b b b W
  ```
- **Panes** are child windows of the frame with an **X border width of 2**. Active/idle state is
  just `XSetWindowBorder`: white (`borderActive`) for the pane containing the pointer, black
  (`borderIdle`) otherwise. Pane geometry resources describe the window without its border
  (matching xldb's `516x364+3+4`).
- **Title bar:** 12px tall (font height - 1), `#0000cd`, text centred in white. With
  `tagWindows`, the tag (`L`, `So`, ...) is drawn at **both** ends. The Messages pane has no
  title.
- **Chips:** a minimized pane becomes a title-only child window at its `IconGeometry`
  (defaults §5.3). Chips never re-flow when others open.
- **Stacking:** autoraise on EnterNotify (`autoRaise`, default true). Middle-click raises when
  autoraise is off. The Source pane is raised whenever execution stops. Menus and dialogs are
  frame children stacked on top.
- **Gestures** (pointer handling shared by all panes, implemented in `pane.c`):
  - left click/drag: pane-specific action. Drag without an object hit = proportional scroll.
  - right press near the centre -> **move**. Near an edge or corner (within a margin to be
    measured, provisionally 1/4 of the smaller dimension) -> **resize** those edges. Both draw a
    1px XOR `0xA5A5A5` rubber band (§5.1) and apply on release.
  - left click on title bar -> Window Control menu.
- **Window Control menu** (`cellmenu`): title `Window Control` + `Restore, Move, Size, Minimize,
  Maximize, Lower, Horizontal scroll bars, Vertical scroll bars, Save Window`. It opens at the
  pane's top-left, the pane's content is blanked while it's open, and the default highlight is
  the last action chosen (initially Minimize). Maximize = the whole frame. Restore = the previous
  geometry. Save Window = the full-width filename dialog, which writes the visible text lines
  with a trailing space each.
- **Menu dismissal:** a click outside closes the menu **and is delivered** to what's underneath
  (observed; kept for fidelity). Escape closes the menu when the pointer is inside it (xldb
  ignored it only because focus was elsewhere).
- **Keyboard focus follows the pointer.** dbxl sets `XSetInputFocus` to the pane under the
  pointer (or the open dialog) so behaviour matches xldb without a window manager.

## 5. Visual constants

### 5.1 Colour roles (24-bit TrueColor values observed)

| Role (resource) | Default | `-bw` | `-wb` |
|---|---|---|---|
| background | `#4876ff` (X `RoyalBlue1`) | white | black |
| foreground | `#ffffff` | black | white |
| titleBackground / chip / dialog background | `#0000cd` | black | white |
| titleForeground | `#ffffff` | white | black |
| borderActive | `#ffffff` | ? | ? |
| borderIdle | `#000000` | black | ? |
| selection bar (current frame, current style) | `#00ffff` bg, black text | black bg, white text | white bg, black text |
| menu item under pointer / default item | `#000000` bg, white text | inverted | inverted |
| menuInfo (`Style:` headers) | `#d0d0d0` | black | white |
| dialog field | `#add8e6` | ? | ? |
| cursorForeground (input cursor box) | `#fa1340` outline | ? | ? |
| stop sign fill | `#fa1340` | ? | ? |
| scroll thumb / arrow fill | `#5151fb` | ? | ? |
| arrow/disabled-stop outline | `#000001` | | |
| arrow fill | `#000000` | | |
| rubber band | XOR `0xA5A5A5` | | |

`?` = not yet measured; capture from `recon/screens/110-bw.png`/`111-wb.png` during
implementation. The resource names from the xldb help are all supported. Note the help's
`RoyalBlue` is wrong: xldb actually renders `#4876ff`. The default must be the literal RGB,
not the colour name, because `RoyalBlue` is `#4169e1` in every modern `rgb.txt`.

### 5.2 Font and metrics
- Default `8x13` (misc-fixed): 8px advance, 13px line pitch. xldb's compiled-in default
  `Rom14.500` never existed on AIX 4.3.3 either, so it always fell back to `8x13`. dbxl uses
  `8x13` directly. `-font`/`dbxl.font` work, and a missing font prints
  `Unable to find font '%s' - using '%s' instead.`
- All layout derives from font metrics (title height = ascent + descent - 1, chip height = title
  height, menu cell height = 19 with `8x13`: to be confirmed as a formula).
- No antialiasing anywhere. Text is drawn with `XDrawImageString`/`XDrawString` core fonts.

### 5.3 Default geometries (from the xldb binary; frame-relative)

| Window | Geometry | Scrollbars | Icon (chip) |
|---|---|---|---|
| frame | 957x846+33+73 | | |
| Locals | 516x364+3+4 | vertical | 100x12+528+4 (hidden while open) |
| Callers | 204x282+528+85 | none | 204x12+528+86 |
| Commands | 204x282+744+85 | none | 149x12+801+85 |
| Source | 947x420+3+377 | vertical | 134x12+3+377 |
| Globals | 771x517+98+111 | vertical | 100x12+634+4 |
| Monitor | 771x517+50+40 | vertical | 100x12+740+4 |
| Files | 277x536+653+259 | vertical | 104x12+846+4 |
| Subprograms | 870x209+38+136 | vertical | 134x12+528+31 |
| Breakpoints | 588x160+363+200 | none | 134x12+672+31 |
| Threads | 784x100+3+127 | none | 134x12+816+31 |
| Disassembly | 567x440+288+377 | none | 134x12+528+58 |
| Registers | 207x440+741+377 | none | 134x12+672+58 |
| Storage | 594x440+4+377 | none | 134x12+816+58 |
| Messages | 947x12+3+826 (bottom, -4) | none | 127x12+693+3 |
| Help | 600x750+0+0 | both | 180x12, bottom-right |
| Formats | 201x460+336+141 | ? | 201x12+336+141 |

Startup state: Locals, Callers, Commands and Source open; everything else as chips; Messages
unmapped until there is a message.

### 5.4 Glyphs (`xtk/glyphs.c`)
Bitmaps are extracted from the recon screenshots at 1:1 and stored as C arrays:
- execution arrow (black, `#000001` outline), drawn **over** the first columns of the line;
- stop sign: 11x12 solid octagon `#fa1340` (*[obs p2]*);
- disabled stop sign: 1px hollow octagon `#000001` (*[obs p3]*);
- pointer: 16x16 white-body/black-outline arrow, hotspot (1,1) (*[obs p6]*, bitmap in the doc);
- scroll arrows, busy pointer, bug icon (the icon partly recovered in `bug_200125e8.png`),
  conditional stop sign (not yet observed; `cond.png` is a placeholder).

## 6. Text formats (core/format.c)

All observed *[obs p2-p5]*. These are the rules the value formatter must reproduce exactly.

| Thing | Format |
|---|---|
| value line | `name: value` |
| signed integers | always signed: `+0`, `+123456789`, `-3` |
| unsigned char / unsigned | `129` (no sign) |
| char | C literal: `'b'`, `'\0'` |
| float / double | shortest scientific: `2.5e-1`, `1.5e+0` |
| enum | enumerator name `GREEN` |
| pointer | `ptr`; `<null>` if 0; after `more`: `->shape{}`; style string: `"hello, world"` |
| array (collapsed) | `[]`; one level: `[ elem elem ]`; more: one element per line `[ 0]: ...` |
| struct (collapsed) | `tag{}`; expanded `{ v v v }` with **no field names**; `flatten` expands all |
| wrap | continuation lines indented to align after `name: [ ` / `{ ` |
| Globals header | `----- File rich.c -----`, blank line, then values |
| Monitor | `----- Globals -----`, blank, `----- Locals -----`, values |
| Callers | `main()`, `__start()`; selected frame cyan |
| Subprograms | sorted, `add()` |
| Breakpoints | `add [line 7 in test.c]` |
| Threads | header `-------Label-------  -Thread-  -Mode-  --Wait--  --wchan-  ----SIGNAL---  --State-`, `    All Threads`, rows |
| Registers | `%-5s: 0x%08x`; FPRs `0x%016x` |
| Storage | `%08X:` then 4 x `%08X`, then 16 chars; unprintable -> `ú` (0xFA) |
| Disassembly | `0x%08x (+0x%04x)  mnem  operands`; first line `(+000000)` |
| Locals title | `Locals for main() in test.c`; `Locals for [unknown]()` when none |
| Source title | the path as found: `./test.c` |

**Linux/x86-64 adaptations (decisions needed, §12):** addresses are 64-bit, and registers and
disassembly are x86-64, not POWER. The column structure is kept and widths grow as needed
(proposal: `%08x` when the address fits in 32 bits, otherwise `%016x`).

## 7. Backend interface (backend/backend.h)

### 7.1 Shape
```c
typedef struct dbg_backend dbg_backend;

struct dbg_backend_ops {
    /* lifecycle */
    int  (*start)(dbg_backend *, const dbg_launch *);   /* program+args, core, attach pid */
    void (*shutdown)(dbg_backend *, int kill_inferior);
    int  (*poll_fds)(dbg_backend *, struct pollfd *, int max);
    void (*on_readable)(dbg_backend *);                  /* parse, queue events */

    /* execution (async; completion arrives as DBG_EV_STOPPED / DBG_EV_EXITED) */
    unsigned (*run_to)(dbg_backend *, const char *func);  /* -r / runTo */
    unsigned (*cont)(dbg_backend *, int signal);          /* signal 0 = Continue */
    unsigned (*next)(dbg_backend *);
    unsigned (*step)(dbg_backend *);
    unsigned (*step_insn)(dbg_backend *);                 /* Machine step */
    unsigned (*finish)(dbg_backend *);                    /* Return */
    unsigned (*restart)(dbg_backend *);
    unsigned (*call)(dbg_backend *, const char *func);    /* commandList sub() */

    /* breakpoints */
    unsigned (*bp_insert)(dbg_backend *, const dbg_location *, const char *cond);
    unsigned (*bp_delete)(dbg_backend *, int id);
    unsigned (*bp_enable)(dbg_backend *, int id, int on);

    /* inspection (results arrive as DBG_EV_RESULT with typed payload) */
    unsigned (*frames)(dbg_backend *, int max);
    unsigned (*select_frame)(dbg_backend *, int level);
    unsigned (*locals)(dbg_backend *, int level, int all);  /* active|all */
    unsigned (*globals)(dbg_backend *);
    unsigned (*value_children)(dbg_backend *, dbg_value_id);
    unsigned (*value_assign)(dbg_backend *, dbg_value_id, const char *text);
    unsigned (*read_memory)(dbg_backend *, uint64_t addr, size_t len);
    unsigned (*registers)(dbg_backend *, unsigned groups);
    unsigned (*disassemble)(dbg_backend *, uint64_t lo, uint64_t hi);
    unsigned (*files)(dbg_backend *);
    unsigned (*functions)(dbg_backend *, int with_debug_only);
    unsigned (*threads)(dbg_backend *);
};
```
Events (`dbg_event`): `STOPPED{reason, signal, bp id, frame}`, `RUNNING`, `EXITED{code}`,
`RESULT{token, payload}`, `ERROR{token, message}`, `OUTPUT{inferior|debugger text}`,
`BP_CHANGED`, `LIBRARY_LOADED` (resolves deferred breakpoints).

**Values** are backend-neutral trees: `{id, name, kind, type_name, size, scalar(bytes or
text), has_children}`. `kind` is `INT_SIGNED | INT_UNSIGNED | CHAR | FLOAT | ENUM | POINTER |
ARRAY | STRUCT | UNION | FUNC | OTHER`. The formatter works from `kind` and raw scalars, never
from backend-formatted strings, so both backends produce identical text.

### 7.2 GDB/MI mapping

| dbxl | GDB/MI |
|---|---|
| launch | `gdb --interpreter=mi3 -nx -q`, `-file-exec-and-symbols`, `-exec-arguments`, `-gdb-set mi-async on` |
| run to `main` | `-break-insert -t main` + `-exec-run` |
| core (`-co`) / attach (`-a`) | `-target-select core` / `-target-attach` |
| Continue / Signal | `-exec-continue` / `-interpreter-exec console "signal N"` |
| Next / Step / Machine step / Return | `-exec-next` / `-exec-step` / `-exec-step-instruction` / `-exec-finish` |
| Restart | `-exec-run` again (breakpoints persist) |
| breakpoints | `-break-insert [-d] [-c cond] [-f] loc`, `-break-delete`, `-break-enable/-disable`; `-f` gives deferred (pending) breakpoints |
| Callers | `-stack-list-frames 0 maxCalls` |
| Locals / Globals / Monitor | var objects: `-var-create - @ expr`, `-var-list-children --all-values`, `-var-update`, `-var-assign` |
| Storage | `-data-read-memory-bytes` |
| Registers | `-data-list-register-names`, `-data-list-register-values x` |
| Disassembly | `-data-disassemble -s lo -e hi -- 0` |
| Files / Subprograms | `-file-list-exec-source-files`, `-symbol-info-functions` |
| Threads | `-thread-info` |

### 7.3 Value kinds from GDB
MI var objects give type *names*, not type *codes*. The GDB backend loads `dbxl.py`, which
defines a Python MI command (GDB >= 13) `-dbxl-value-info VAROBJ` returning
`kind, size, bytes, enum names` from `gdb.Value`/`gdb.Type.code`. If GDB lacks Python, the
fallback classifies by parsing `ptype`/`whatis` output, which is lossier. A future LLDB backend
gets the same information natively from SBValue/SBType.

### 7.4 LLDB (future)
Implement `dbg_backend_ops` over `lldb-dap` (DAP over stdio). `lldb-mi` is out of tree and
poorly maintained. Nothing above `backend/` may reference MI concepts (tokens, varobj names,
async records).

## 8. Panes: behaviour summary

Each pane is a `textpane` (list of styled lines + hit-test) plus a click handler:
- **Source:** left-click a line = toggle breakpoint (message `Cannot breakpoint line N of `F'`
  when not possible). Glyphs in the left margin, drawn over the text. When stopped: scroll the
  line into view and raise Source. A selected caller frame's line is drawn as a full-width cyan
  bar. `:n`, `/x`, `\x`, `?x` via the inline LightBlue input strip at the pointer row. The match
  or line becomes the **pointer position** (warp), not a caret.
- **Disassembly:** titled with the function name, the same glyph and cyan conventions, and
  Machine step moves only this arrow.
- **Locals/Globals/Monitor:** value trees. Left-click an item (or element) opens the type menu
  (§9).
- **Callers:** click selects a frame and updates Locals, Source and Disassembly.
- **Commands:** from `commandList` (default below). A name column plus a right-aligned `Alt-x`
  column when a key is defined. `null` rows are plain text.
  ```
  Continue c, Next n, Step s, Machine step m, Return r, Signal g, Edit e,
  Restart t, Exit x, Breakpoint b, Options o, Help h, ""=null
  ```
- **Breakpoints:** click selects (cyan) and scrolls Source/Disassembly to it, then opens the
  `Select Breakpoint Action` cell menu (Clear/Disable/Enable, and the `all` variants).
- **Storage:** hex dump. `Storage view` scrolls the target to the top row and messages
  `Storage target address is: %x`. When Storage is hidden, the message is
  `Storage view ignored. Storage pane is hidden`.
- **Registers:** special set, GPRs, then FPRs when doubles are on (Options -> Register control
  -> `Display controls` toggles; the menu stays open while toggling).
- **Messages:** unmapped until a message arrives, then shown as a title-coloured bar with
  centred text. It clears on the next action.
- **Help:** one long document, `-->` links scroll to their sections, and the title-bar
  `[exit][index][return]` hotspots give close, index and back.
- **Options menu:** the 15 items and values from *[obs p1]*. Clicking cycles values
  (right-click cycles backwards).
- **Dialogs:** Exit `Exit from dbxl?` yes/no (centred). Breakpoint `Enter function name`,
  Save Window `Enter the target file name` and `Edit source search path` are full width.
  Edit value `Enter new value:` is compact, at the menu spot, pre-filled. Enter = proceed,
  Esc = cancel.

## 9. Variable menus
List-box menus at a fixed frame position (observed at (338,143), ~205x463), 1px white border,
title = kind (`int`, `char`, `array`, `structure`, `pointer`). Section headers in menuInfo
colour. One item pre-highlighted (the current style for scalars, `more` for array/pointer,
`flatten` for structs). Item lists per kind are exactly as recorded in *[obs p3, p4]*.
Styles and detail levels are stored per value path, so they survive stepping and re-expansion.

## 10. Help text
`help/dbxl.help` is **dbxl's own text**, organised like xldb's help (same section numbering,
`-->` link syntax, index, "copy help text to `$HOME/dbxl.help.text`"). IBM's text in
`recon/xldb-h.txt` is IBM's copyrighted material. We use it to understand structure and
behaviour, but we don't copy it into dbxl. `-h` prints the help to stdout.

## 11. Deliberate deviations from xldb
| xldb | dbxl |
|---|---|
| "xldb" in every string | "dbxl" |
| first Continue stops with SIGCONT (AIX/qemu artifact) | not reproduced |
| stale fragments after dialogs close | full repaint |
| `-wb` execution arrow invisible (black on black) | **open question**: faithful or fixed |
| POWER registers/disassembly | host architecture (x86-64 first) |
| cannot debug dynamically linked programs on AIX 4.3 | not applicable |

## 12. Open questions
1. `-wb` arrow: reproduce the invisible arrow, or draw it in the foreground colour?
2. Address width in Disassembly/Storage on 64-bit targets (proposal in §6).
3. x86-64 register groups: which registers are "general", "special" and "double"?
4. Missing measurements: `-bw`/`-wb` role colours, the resize edge margin, the conditional stop
   sign glyph, the busy pointer, the Formats window, Signal, `-q`, core files.
5. Does "Size" from the Window Control menu start an outline immediately (help text), or wait
   for a press? Recon didn't confirm it.

## 13. Testing
- **Unit** (`make test`): MI parser against captured GDB transcripts, formatter against the
  table in §6, commandList/resource parsing, search.
- **Visual** (`make visual`): run dbxl under `Xvfb :43 -screen 0 1280x1024x24` with scripted
  xdotool interactions mirroring the recon scenarios (tests/progs ports of `test.c` and
  `rich.c`). Compare crops against `recon/screens/*` pixel by pixel, where content can match
  (chrome, menus, dialogs, value text). Report diffs as zoomed PNGs.
- **Reference captures** stay in `../recon/`. Copy only the ones tests use into
  `tests/visual/ref/`.

## 14. Milestones
1. **Skeleton:** Makefile, event loop, frame with stipple, panes with borders and titles,
   chips, static default layout. Visual test: the startup screenshot minus content.
2. **xtk complete:** menus (both kinds), dialogs, input strip, scrollbars, rubber band,
   autoraise, pointer glyph and warping, resources and `-bw`/`-wb`.
3. **GDB backend:** spawn, MI parser, run to main, Source + arrow + Commands (Continue, Next,
   Step, Return, Machine step, Restart, Exit), Messages.
4. **Data panes:** Locals, Globals, Monitor with formatter and variable menus. Callers, frame
   selection.
5. **Remaining panes:** Breakpoints window and actions, Disassembly, Registers, Storage, Files,
   Subprograms, Threads, Help (own text), Options menu, Save layout, Save Window.
6. **Hardening:** core files, attach, signals, deferred breakpoints, `commandList` actions,
   sample layouts. Then evaluate an LLDB backend.
