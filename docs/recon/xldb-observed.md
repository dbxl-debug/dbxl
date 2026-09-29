# xldb 1.2.1.0: observed behaviour (recon pass 1)

> **Where things are.** These notes were written in the reference folder's `recon/`
> directory, and the paths in them are relative to it. The screenshots (`screens/`), the
> captures (`m2ref/` ... `m8scen/`, `p17/`), the X traces (`xtrace/*.log`, `*.txt`) and
> IBM's own text (`xldb-h.txt`, `xldb-messages.txt`) stay there, outside this repository.
> What moved into the repository: the notes themselves and `bitmaps-decoded.txt`
> (`docs/recon/`), the recon tools (`tools/recon/`; `tools/edgeprobe*.py`,
> `tools/zoneprobe.py` and `tools/tracedrag.py` were one-off probes and were not kept), and
> the test programs (`testprog/` is now `tests/progs/` and `tests/progs/aix/`). See
> `docs/RECON.md` for how to run and record xldb.

Source of truth for dbxl. Where this disagrees with `xldb-ui-reconstruction-spec.md` or
`xldb-visual-archaeology.md`, this wins.

**Setup:** AIX 4.3.3 guest (192.168.76.2), `xldb /usr/bin/bsh` (no `-g` info), displayed on
host `Xvfb :42 -screen 0 1280x1024x24`. No window manager. No `.Xdefaults`. Date: 2026-09-27.
Screenshots are in `screens/`; the full `xldb -h` help text is in `xldb-h.txt`.

## Identity and linkage
- `what` on the binary: `xldb version 1.2.1.0 IBM XLDB Source Level Debugger`. `/usr/lpp/xldb/bin/xldb`, 1215508 bytes, dated 8 Mar 1996, cksum 693052378.
- `dump -H`: imports only **libc.a, libC.a (xlC C++ runtime), libX11.a**. No Xt, no Motif. The UI is xldb's own `Wintk/Xtk` layer on raw Xlib. **Confirmed.**
- Top-level window properties: `WM_NAME "xldb bsh"` (i.e. `xldb <program basename>`), `WM_ICON_NAME "xldb"`, `WM_CLASS "xldb","xldb"`, `WM_COMMAND {"xldb"}`, USPosition/USSize 957x846+33+73, min size 64x64, icon pixmap set in WM_HINTS.

## Big correction: one frame, not independent top-levels
The specs say each xldb window is an independent top-level X window. **Wrong.** xldb creates
**one** top-level frame (957x846). Every "window" (Locals, Source, Callers...) is a **child X
window inside that frame**, positioned with the per-window geometry. Moving, sizing, raising and
lowering all happen inside the frame, and xldb draws all the chrome itself.

The frame background is a **4x4 stipple** of white and RoyalBlue1:
```
W b W b
b W b b
W b W b
b b b W
```

## Default layout (no resources)
The default geometries are compiled into the binary as strings (`957x846+33+073`,
`516x364+003+004` ...) and match `screens/01-tree.txt`.

Visible at startup (see `screens/01-startup.png`):
| Window | Geometry in frame | Notes |
|---|---|---|
| Locals | 516x364+3+4 | title `Locals for [unknown]()`, vertical scrollbar |
| Callers | 204x282+528+85 | `__start()` selected (cyan) |
| Commands | 204x282+744+85 | 12 commands, see below |
| Source | 947x420+3+377 | vertical scrollbar, black arrow glyph at the current line |

Minimized ("chip") windows: a 12px title-only bar in the upper right, 3 rows:
- Row 1: `Globals` 100x12+634+4, `Monitor` 100x12+740+4, `Files` 104x12+846+4
- Row 2: `Subprograms` 134x12+528+31, `Breakpoints` +672+31, `Threads` +816+31
- Row 3: `Disassembly` 134x12+528+58, `Registers` +672+58, `Storage` +816+58

Clicking a chip opens that window at its own default geometry, overlapping the others. The chip
disappears and the **remaining chips do not re-flow**.

Other windows seen: Help 600x750+0+0 (both scrollbars), Registers 207x440+741+377,
Disassembly 567x440+288+377, Storage 594x440+4+377, Messages 947x12 at bottom.

## Colours (exact RGB on a 24-bit TrueColor server)
| Role | RGB | Notes |
|---|---|---|
| Pane background | `#4876ff` | = X `RoyalBlue1`, **not** `RoyalBlue` (`#4169e1`) as the help says |
| Text foreground | `#ffffff` | |
| Title bar / chips / dialog background | `#0000cd` | = `MediumBlue` |
| Borders | `#000000` | 2px black around panes |
| Selected line (Callers) | `#00ffff` bg, black text | cyan |
| Menu item under pointer | `#000000` bg, white text | |
| Dialog border / dialog button border | `#ffffff` | |
| Dialog buttons | `#4876ff` fill | |
| Dialog text field | `#add8e6` | = `LightBlue` |
| Text cursor box in field | `#fa1340` outline | |
| Scrollbar trough | stipple (same 4x4 pattern) | 9px wide, 9x9 arrow buttons |

Binary strings include `RoyalBlue`, `White`, `Black`, `white`, `black`. Why the server shows
RoyalBlue1 is not yet explained. The observed value is what matters.

**Hover:** moving the pointer into a pane changed **no** pixels. No visible active or idle border
change with the default resources.

## Font
- The compiled-in default font is **`Rom14.500`**, an AIX 3-era name. It does **not** exist in
  AIX 4.3.3's `X11.fnt` package either, so on a real 4.3.3 display xldb also falls back.
- The fallback is **`8x13`** (binary message: `Unable to find font '%s' - using '%s' instead.`).
  Everything in the screenshots is 8x13: 8px advance, 13px line pitch.
- `8x13` is the standard misc-fixed font on Linux too. **Recommendation: dbxl defaults to 8x13.**
- The AIX fonts (including `Rom14.iso1.pcf.Z` = `-IBM-Roman-Medium-R-Normal--20-140-100-100-C-90-ISO8859-1`)
  can be extracted from `vm/Volume_1.iso` `X11.fnt` with `tools/unbff.py` if we ever want to see
  xldb with its intended font.

## Commands window
Default list compiled into the binary (`R_default_commands`):
```
Continue Alt-c | Next Alt-n | Step Alt-s | Machine step Alt-m | Return Alt-r | Signal Alt-g
Edit Alt-e | Restart Alt-t | Exit Alt-x | Breakpoint Alt-b | Options Alt-o | Help Alt-h
```
followed by `""=null`. There are **12** entries. The spec's 10-entry `commandList` is wrong (it
omits Machine step and Breakpoint), and "Stop" is not in the default list. Accelerators are shown
right-aligned as `Alt-x`. `R_short_commands` = `Edit Exit Options Help` also exists (probably
the `-q` or no-program mode).

## Window Control menu (left-click a title bar)
`screens/03-window-control-menu.png`. Title `Window Control`, then 9 items, centred text, each
in its own bordered cell:
`Restore, Move, Size, Minimize, Maximize, Lower, Horizontal scroll bars, Vertical scroll bars, Save Window`
(the spec says "Save"). It opens at the pane's top-left and is a child of the frame.
- **Escape did not dismiss it**, and clicking its title did not either.
- **Clicking outside dismissed it AND the click went through** to the pane underneath: a
  Source-pane click tried to set a breakpoint.

## Options menu
`screens/08-options-menu.png`. Title `Options`. Items with right-aligned current values:
```
Save layout
Edit source search path
Local variables:            Active
Detail per click:           1
Multiprocess debugging      yes
Fork path:                  Parent
Autoraise:                  yes
Compiler variables:         Hide
Language:                   Automatic
Register control
Subprograms:                With -g  only
Save Breakpoints
Load Breakpoints
Case Sensitive              no
Breakpoint all Subprograms  no
```
The menu is placed at the pointer and clamped to stay inside the frame. The first item is
highlighted when it opens.

## Dialogs
- **Exit** (`screens/10-exit-dialog.png`): `#0000cd` box with a white border, centred in the
  frame, text `Exit from xldb?`, buttons **`yes`** / **`no`** stacked on the right. The help
  says OK/Cancel, which is wrong.
- **Breakpoint** (`screens/17-breakpoint-dialog.png`): full frame width. Prompt
  `Enter function name`, LightBlue text field with a red cursor box, buttons **`proceed`** /
  **`cancel`**.

## Messages window
Unmapped until there's a message. It then appears as a `#0000cd` bar at the bottom of the frame
with **centred** white text, e.g. `Cannot breakpoint line 18 of `'`. It cleared on the next
action.

## Registers, Disassembly, Storage
- **Registers:** one per line, `%-5s: 0x%08x` style (`IAR  : 0x10000100`, `GPR10: 0xdeadbeef`).
  The order is IAR, MSR, CR, LR, CTR, XER, MQ, TID, FPSCR, GPR0..GPR31. It has no scrollbar by
  default.
- **Disassembly:** the window title is the **current function name** (`unknown function name`
  for stripped `bsh`). It was empty for `bsh`.
- **Storage:** the title is **`Storage Pane`**. Each row is `ADDRESS:` then 4 big-endian hex
  words, then 16 ASCII chars, with unprintables shown as **`ú`** (0xFA).

## Help window
The title is `Help [exit][index][return]`, where the bracketed words are clickable. The index
page matches `xldb-h.txt`, and `-->` lines are links. The last line offers to copy the help to
`$HOME/xldb.help.text`. For dbxl, rename every user-visible "xldb" to "dbxl".

## Open items for the next pass
- Needs a `-g` program: Source, Locals, Globals, Files, Subprograms, Breakpoints (stop-sign
  glyphs), Callers with real frames, Monitor, Threads.
- Menu and dialog keyboard handling. Test with a window manager, or by setting focus explicitly.
  No WM was used here.
- Move and Size gestures (right-drag), maximize/minimize/lower, scrollbar arrow glyphs close up,
  the custom cursor (mouseBody/mouseOutline), the bug icon pixmap.
- Machine step to see Disassembly populate. Monitor, Threads, Files and Subprograms chips.
- `-bw` / `-wb` schemes, `tagWindows`, and the sample layouts.
- Explain RoyalBlue vs RoyalBlue1.

---

# Recon pass 2: a `-g` test program (hand-written assembly)

## Building debuggable test programs without a compiler
`tests/progs/test.c` is a 21-line C program. `tests/progs/aix/test.s` is hand-written PowerPC
assembly with XCOFF stabs that mimics `xlc -g` output. Build on the guest (needs `bos.adt.base`
and `bos.adt.syscalls`, both now installed):
```
as -o test.o test.s
ld -o tests -bnso -bI:/usr/lib/syscalls.exp -bpT:0x10000000 -bpD:0x20000000 /usr/lib/crt0.o test.o -lc
```
- **Link statically (`-bnso`).** xldb 1.2.1.0 (1996) **cannot debug dynamically linked programs
  on AIX 4.3.3**. It exits with `Unable to find member shr.o of /usr/lib/libcrypt.a`, because
  4.3's shared libraries use the newer `<bigaf>` archive format. `-n` doesn't avoid it. `bsh`
  works only because it's static.
- **Stab placement that xldb accepts** (anything else and xldb drops the function from
  Subprograms and Step steps over it):
  ```
  .foo:
      .function .foo,.foo,16,044,FE..foo-.foo
      .stabx "foo:F-1",.foo,142,0      # C_FUN
      .bf    <abs line of {>
      .stabx "a:p-1",<off>,130,0       # C_PSYM params, AFTER .bf
      .stabx "x:-1",<off>,129,0        # C_LSYM locals
      .line  <line - bf + 1> ...
      ...code, traceback table...
      .ef    <abs line of }>
  FE..foo:
  ```
  Globals: `.stabx "counter:G-1",0,128,0`. Type `-1` = predefined int.
- AIX `dbx` (from `bos.adt.debug`) is useful for cross-checking stabs.

## Observed with debug info (`screens/22`..`33`)
- **Startup:** runs to `main` and stops on `main`'s `{` line (`.bf` line). The Source title is
  the file path as found (`./test.c`). Locals title: `Locals for main() in test.c`. Callers:
  `main()`, and `__start()` once deeper.
- **Value format:** `name: value`, with signed ints shown with an explicit sign (`i: +0`).
  Params are listed before locals, in declaration order.
- **Globals:** grouped by file, with a header line `----- File test.c -----`, a blank line,
  then `counter: +0`.
- **Subprograms:** `add()`, `main()`, sorted alphabetically, with `()` appended. Default
  geometry 870x209+38+136.
- **Breakpoints:** left-click a source line to set one. The marker is an **11x12 solid octagon
  in `#fa1340`**, drawn in the left margin. The execution arrow is drawn **over** it when
  stopped there. Message: `Breakpoint encountered.`
- **Execution arrow:** a black right-pointing arrow drawn in the left margin **over** the first
  characters of the line.
- **Scrolling:** when stopped, the Source view scrolls so the current line is visible.
- **Active border:** the pane containing the pointer gets a **white** 2px border, the others
  black. (Pass 1's hover test didn't show this; xdotool's first move may not have generated an
  EnterNotify.)
- **Autoraise:** entering a pane raises it above overlapping panes (Commands came in front of
  Subprograms).
- **Quirk:** the first Continue after startup always stops with
  `Program stopped during trace by signal 19: SIGCONT (continue).` A second Continue proceeds
  normally. Probably an artifact of this AIX/qemu setup, **don't copy into dbxl**.
- **Quirk:** some command clicks seemed to be absorbed (Step needed two clicks). Unclear whether
  that's xldb or timing on the emulator.

---

# Recon pass 3: windows, menus and gestures with debug info (`screens/40`..`56`)

## Window Control actions
- The menu opens at the pane's top-left with **`Minimize` pre-highlighted** (black bar) as the
  default item, whatever the pointer position. While the menu is up, the target pane's
  **contents are blanked** (background only).
- **Minimize** puts the window back into its original chip slot (chips don't re-flow).
- **Move = right-button drag** anywhere in a pane. While dragging, a 1px rubber-band rectangle
  of the pane's full outline follows the pointer, drawn with **XOR `0xA5A5A5`** (so on
  `#4876ff` it appears `#edd35a`). The pane moves on release and keeps its white active border.
- **Autoraise:** moving the pointer into any visible part of a pane raises it above the others.

## Disassembly (`42`, `43`)
- The title is the function name (`add`, `main`). No scrollbars by default.
- Line format: `0x10047104 (+000000) stwu      r1,0xFFFFFFC0(r1)`
  - The address is `0x%08x` lowercase. The offset is `(+0x%04x)`, **except the first line, which
    is `(+000000)`**.
  - The mnemonic is in a fixed column (col 22), operands at col 31.
  - Registers are `rN`. Displacements are `0x%04X(rN)`, immediates `0x%08X` (uppercase hex).
  - `blr` is shown as `bclr BO_ALWAYS,CR0+LT`, a conditional branch as
    `bc BO_IF_!,CR0+GT,0x10047168`, `nop` (`ori 0,0,0`) as `mr r0,r0`, and `cmpwi` as
    `cmpi 0x0000,0,r0,0x00000004`. Calls are `bl add` (symbolic).
- Breakpoint and arrow glyphs are drawn in the left margin, the same as in Source.
- **Machine step** moves the Disassembly arrow one instruction. The Source arrow stays on the
  line.

## Variable menu (left-click a Locals entry) (`44`, `45`)
A list box with a **1px white border** (not the Window-Control cell style), titled with the
**type name** (`int`). Contents:
```
Style:              <- menuInfo colour #d0d0d0, not selectable
    - character
    - signed         <- current style, cyan bar
    - unsigned
    - hex
    - address
    - type
    - size
    - save
    - recall
    - default
Edit
Breakpoint
Monitor on/off
Function parameter
Storage view
```
It's fixed size (~205x463) with empty space below. Choosing `hex` shows `a: 0x00000000`.

## Callers and frame selection (`46`)
Clicking `main()` selects that frame. Locals switches to `Locals for main() in test.c`,
Disassembly shows `main` with the call instruction (`bl add`) as a **full-width cyan bar**, and
Source shows the caller's line as a **full-width cyan bar**. The black execution arrow stays at
the real stop point in `add`.

## Breakpoints window (`47`..`49`)
- Default geometry 588x160+363+200. Entry format: `add [line 7 in test.c]`.
- Clicking an entry gives it a cyan bar, and Source and Disassembly scroll to it and highlight
  it in cyan. It then pops a menu titled **`Select Breakpoint Action`** (Window-Control cell
  style, first item pre-highlighted):
  `Clear breakpoint, Disable breakpoint, Enable breakpoint, Clear all breakpoints, Disable all breakpoints, Enable all breakpoints`.
- **Disabled breakpoint glyph:** a **hollow octagon outline**, 1px, colour `#000001`. The
  execution arrow's outline is also `#000001`, with a `#000000` fill.

## Files, Threads, Monitor (`50`..`52`)
- **Files:** a list of source files (`test.c`). Default 277x536+653+259, vertical scrollbar.
- **Threads:** header row
  `-------Label-------  -Thread-  -Mode-  --Wait--  --wchan-  ----SIGNAL---  --State-`,
  then `    All Threads`, then one row per thread with the current one in cyan:
  `6453   user   Breakpoint-05   Enabled`.
- **Monitor:** `----- Globals -----`, a blank line, `----- Locals -----`. It's empty until
  variables are monitored (via the variable menu's `Monitor on/off`). Vertical scrollbar.

## Return (`56`)
Runs until the function returns and stops at the **next** line in the caller (line 18
`counter++`), not on the call line.

---

# Recon pass 4: richer data types (`testprog/rich.c`, `screens/60`..`85`)

## More build rules learned (all needed for xldb to show globals correctly)
1. **Type declarations** (`.stabx "name:T..",0,140,0` C_DECL) must come **after a `.csect`**.
   Before the first csect the assembler silently drops them.
2. **One csect per global** (`.csect square[RW],3`), as xlc does, referenced as `square[RW]`
   (TOC: `.tc box[TC],box[RW]`).
3. **Each global's `.stabx "g:G.."` must sit inside that global's own csect.** xldb binds a
   C_GSYM to the csect it appears in. When all the stabs followed the last csect, xldb showed
   only the first global and read `counts`' memory for it.
4. **Keep unreferenced globals:** AIX `ld` garbage-collects unreferenced csects. Use
   `-bkeepfile:rich.o` (`-bnogc` drags in all of libc and fails).
   Full link: `ld -o rich -bkeepfile:rich.o -bnso -bI:/usr/lib/syscalls.exp -bpT:0x10000000 -bpD:0x20000000 /usr/lib/crt0.o rich.o -lc`
5. Use POWER/common mnemonics (`sf` not `subf`). The default assembly mode is COM.

## Value display formats (Locals / Globals)
| Type | Collapsed (default) | Notes |
|---|---|---|
| int, short, long | `+123456789`, `-3`, `+0` | always signed with explicit `+` |
| unsigned char | `129` | unsigned, decimal, no sign |
| char | `'\0'`, `'b'` | C char literal |
| float | `2.5e-1` | scientific with minimal digits |
| double | `1.5e+0` | |
| enum | `GREEN` | |
| pointer | `ptr`, or `<null>` when 0 | |
| pointer after `more` | `->shape{}`, `->point{}` | `->` + collapsed target |
| char * with style `string` | `"hello, world"` | |
| array | `[]` | |
| struct | `shape{}` | tag name + `{}` |

Expanding (`more`, one level per click with detailPerClick 1):
- array: `square: [ point{} point{} point{} point{} ]`, then one element per line:
  ```
  square: [ [ 0]: { +1 +2 }
            [ 1]: { +3 +4 }
            ...                 ]
  ```
  Index format `[%2d]:`. Continuation lines are indented to line up after `name: [ `.
- struct: `box: { [] GREEN point{} ptr 4 1.5e+0 }`. **Field names are never shown**, only
  values in declaration order.
- `flatten` expands all nested levels (not through pointers):
  `box: { [ 'b' 'o' 'x' '\0' ... ] GREEN { +5 +5 } ptr 4 1.5e+0 }`, wrapped onto an indented
  continuation line.
- Each element of an expanded aggregate is individually clickable and gets its own
  type-specific menu (clicking a `'\0'` gives a `char` menu).

## Variable menus by type (all at the same fixed spot, frame ~(338,143), ~205x463, 1px white border)
- **int** (title `int`): `Style:` character/signed/unsigned/hex/address/type/size/save/recall/default, then Edit, Breakpoint, Monitor on/off, Function parameter, Storage view.
- **char** (title `char`): same as int.
- **array** (title `array`): `Detail:` more/less, `Style:` string/address/type/size/save/recall/default, then Select subrange, Monitor on/off, Function parameter, Storage view.
- **structure** (title `structure`): `Detail:` more/less/flatten, `Style:` address/type/size/save/recall/default, then Show self, Monitor on/off, Function parameter, Storage view.
- **pointer** (title `pointer`): `Detail:` more/less, `Style:` hex/string/array/pointer/address/type/size/save/recall/default, then Breakpoint, Select subrange, Edit, Cast, Downcast, Monitor on/off, Function parameter, Storage view.
- Section headers (`Detail:`, `Style:`) are in menuInfo colour `#d0d0d0`. One item is
  pre-highlighted in cyan: the current style for scalars (`signed`), `more` for
  arrays/pointers, `flatten` for structs.

## Edit dialog (`80`..`82`)
Choosing `Edit` gives a dialog **at the menu spot** (not full width, ~393px): `Enter new value:`,
a LightBlue field pre-filled with the current value (`+0`) and the red cursor box,
`proceed`/`cancel`. **Keyboard works when the pointer is over it** (focus follows the pointer):
BackSpace, digits and Return applied `w: +42`.

## Keyboard in Source (`83`..`85`)
With the pointer in Source, typing `:` opens an **inline LightBlue input strip across the pane
at the pointer's row**, showing `:` plus the red cursor box. `:45` Return scrolled the view (line
45 was already visible; it scrolled one line). `/counts` Return made no visible change. The
text-cursor rendering and search highlighting are **not yet understood**.

## Misc
- When execution stops, the **Source window is raised** above overlapping panes.
- xldb leaves stale fragments after dialogs and lowered windows (a repaint glitch). Don't
  replicate.

---

# Recon pass 5: remaining windows, gestures, commands, schemes (`screens/90`..`112`)

## Monitor and Storage view
- Variable menu **Monitor on/off** adds the variable to Monitor under its section:
  `----- Locals -----` / `w: +42`.
- **Storage view** needs the Storage window open. Otherwise the Messages bar says
  `Storage view ignored. Storage pane is hidden`. When it's open, Storage scrolls so the target
  address is the **top row** (`20014A80:`) and Messages says
  `Storage target address is: 20014a80` (lowercase there, uppercase in the pane). The target
  isn't otherwise highlighted.

## Window Control: Maximize / Restore / Size
- **Maximize:** the pane fills the **entire frame** (0,0 to 957x846), covering chips and
  everything else.
- **Restore:** returns it to its previous geometry.
- **Default highlighted item is sticky:** it's the last action chosen (Minimize initially,
  Maximize after maximizing).
- **Size / resize:** a **right-button drag starting near an edge or corner** (right-drag near
  the centre = Move). The XOR `0xA5A5A5` outline shows only the dragged edge(s) moving, and the
  new size applies on release. The help says the `Size` menu action works as "drag the outline,
  then click any button". My left-drag after choosing Size did nothing, so that path is still
  unverified.
- Releasing over another pane makes it autoraise.

## Scrollbars (Source, zoom `z-scroll-*.png`)
- Vertical bar **9px** wide, at the pane's right edge, inside the 2px border.
- **Trough:** 1x1 checkerboard of white and `#4876ff` (not the frame's 4x4 stipple).
- **Thumb:** solid `#5151fb` with 1px black lines top and bottom, full bar width.
- **Arrow buttons** (9x9): a black-outlined triangle filled `#5151fb` on the trough pattern,
  with a black line separating it from the trough.
- Clicking the trough above or below the thumb **pages** up or down.

## Commands
- **Next:** steps over the line (`w` updated to `+10`). **Changed values are not highlighted.**
- **Restart:** reloads and stops at `main`'s `{` again. Breakpoints are kept. No message.
- **Exit -> yes:** xldb and the debuggee exit quietly. No stdout/stderr output.

## Colour schemes
- **`-bw`:** pure black/white. Panes are white with black text. Title bars and chips are black
  with white text. Selection and the default menu item are an inverted black bar. Frame stipple
  is black/white. Menu-info headers (`Style:`) are black like the rest (no grey). The breakpoint
  and arrow glyphs are presumably black.
- **`-wb`:** inverse: black panes, white text, white title bars and chips with black text.
  **The execution arrow is invisible** (always drawn black, `#000001` outline and `#000000` fill,
  on the black background). Faithful-to-a-fault detail.

## Resources, tags and sample layouts
- xldb reads **`$HOME/.Xdefaults` itself** (client side; our Xvfb has no RESOURCE_MANAGER).
  `-name messy-xldb` selects the `messy-xldb.*` resources.
- The messy sample (`testprog/Xdefaults.messy`, extracted from the help text) applies: frame
  `+35+150`, per-window geometries, and IconGeometry chip positions.
- **`tagWindows: true`:** the tag is drawn at **both ends** of the title bar, left and right
  (`L` for Locals, `So` for Source).
- **Custom `commandList`:** entries in order, and `"________"=null` rows render as literal
  underscores. Commands without `:key` show **no Alt column**.

---

# Recon pass 6: cursor, save, options submenus, help navigation (`screens/121`..`132`)

## The "cursor" is the mouse pointer
xldb has no text cursor in Source. Its "cursor" is the **X pointer**, which it warps:
- **Arrow keys** warp the pointer by one character cell (Right +8px, Down +13px), snapping to
  the cell centre. **Home/End** warp to the first/last column of the line.
- **`/string` Return** warps the pointer to the match (`counts` on line 47). `/` Return repeats.
  **`:n` Return** scrolls line n into view.
- **Pointer glyph** (`screens/xldb-pointer-16x16.png`, via XFixes): a 16x16 arrow, **white body
  (`mouseBody`), black outline (`mouseOutline`)**, hotspot (1,1). This is the inverse of X's
  standard `left_ptr`. Exact bitmap:
  ```
  ##..............
  #W#.............
  #WW#............
  #WWW#...........
  #WWWW#..........
  #WWWWW#.........
  #WWWWWW#........
  #WWWWWWW#.......
  #WWWWWWWW#......
  #WWWWW####......
  #WW#WW#.........
  #W#.#WW#........
  ##..#WW#........
  .....#WW#.......
  .....#WW#.......
  ......###.......
  ```
- Screen captures don't include the pointer, which is why passes 4 and 5 "saw nothing".
- The red box in dialogs and input strips (`#fa1340`) is the **input cursor**
  (cursorForeground/Background).

## Save Window
Window Control -> `Save Window` opens a **full-width** dialog `Enter the target file name`
(proceed/cancel). The file gets the pane's text lines **each with a trailing space**, no title:
```
sp: <null> 
a: +0 
tag: '\0' 
```

## Options submenus
- **Register control** replaces the Options menu with a menu titled **`Display controls`**:
  `General registers: Show`, `Double registers: Hide`, `Special registers: Show` (the help says
  special defaults to Hide, which is wrong). Clicking a toggle flips the value and **keeps the
  menu open**, with the highlight moving to that row.
- **Edit source search path:** a full-width dialog `Edit source search path`, pre-filled with
  `. %o `.

## Registers details
Order: IAR MSR CR LR CTR XER MQ TID FPSCR (the "special" set, shown by default), GPR0..GPR31,
then with doubles on `FPR0 : 0x0000000000000000` (16 hex digits) through FPR31. The label
column is 5 wide plus `:`.

## Left-drag scrolling
Holding the left button and dragging in a pane scrolls the contents with the pointer
(proportional). Works in panes without scrollbars (Registers).

## Help navigation
The help is one long document. Clicking a `-->` line scrolls so that section's heading is the
top line. Title-bar `[return]` goes back (history), `[index]` goes to the index, `[exit]`
closes Help.

---

# Recon pass 7: `-wb` / `-bw` glyph and role colours (`screens/140`..`157`)

Setup: `rich`, breakpoints on lines 33 and 34, stopped at 33, then line 34 disabled.

## Glyphs in the monochrome schemes
Every glyph colour collapses to **black**. The outline stays `#000001`, and every fill (arrow,
enabled stop sign, disabled stop sign) is `#000000`:

| Glyph | default | `-bw` (white bg) | `-wb` (black bg) |
|---|---|---|---|
| execution arrow | black fill, `#000001` outline | same, visible | same, **invisible** |
| enabled stop sign | `#fa1340` solid octagon | **solid black octagon** | **invisible** |
| disabled stop sign | hollow `#000001` outline (bg shows through) | **solid black octagon**, identical to enabled | **invisible** |

Consequences:
- `-bw`: breakpoints and the arrow are visible, but **enabled and disabled breakpoints look
  identical** (confirmed after a forced repaint).
- `-wb`: **nothing** in the margin is visible: no arrow, no breakpoints. A glyph only shows when
  it sits on a white highlight bar (e.g. the selected Breakpoints entry's line), as a black
  octagon.
- In the default scheme the disabled glyph's interior is the pane background, but in `-bw` it's
  black. So xldb isn't "filling with background". It uses a fixed fill pixel that is black in
  both mono schemes.

## Role colours
| Role | `-bw` | `-wb` |
|---|---|---|
| pane bg / text | white / black | black / white |
| title bar, chips, menu title | black bg, white text | white bg, black text |
| selection bar (Callers, Breakpoints entry, source cyan line) | black bar, white text | white bar, black text |
| menu default/hover item | black bar, white text | white bar, black text |
| menu cells | white bg, black text, black cell borders | black bg, white text, white cell borders |
| list-menu border | black 1px | white 1px |
| menuInfo (`Style:`) | same as text (black) | same as text (white) |
| dialog background | black | white |
| dialog text | white | black |
| dialog text field | **black (same as dialog bg, no visible field)** | **white (same as dialog bg)** |
| input cursor box | white outline | black outline |
| dialog buttons | white bg, black text, black edge | black bg, white text, white edge |
| scrollbar | black/white only (thumb pattern TBD) | black/white only |
| frame stipple | black/white | black/white |

## Also found
- The Breakpoints window marks disabled entries with a suffix: `area [line 34 in rich.c]  Disabled`
  (two spaces before `Disabled`). This applies to all schemes.

---

# Recon pass 8: mono borders, resize zones, Size menu (`screens/172`..`174`, tools/edgeprobe*.py)

## Frame border
The top-level frame has a **2px X border**: **black** in the default scheme and `-bw`, white in
`-wb`. The frame's inside origin is at the frame position + 2 (e.g. root (35,75) for
`+33+73`). `xwininfo`'s "absolute upper-left" is the outer corner of the border.

## Pane borders in mono schemes
- `-bw`: pane borders are **black whether active or idle** (no active indicator).
- `-wb`: pane borders are **white whether active or idle**.
- Default scheme: idle black, active white (pass 2).

## Scrollbars in mono schemes (`-bw`)
Checkerboard trough, **solid black thumb**. Arrow buttons are a black triangle outline with the
trough pattern showing through (not filled). Black separator lines, as in the default scheme.

## Right-drag move vs resize (partially measured)
Probing with scripted right-drags (`tools/edgeprobe2.py`):
- Pressing near an edge resizes, and pressing near the middle moves. The boundary is **not a
  fixed fraction or pixel margin**:
  - Callers (208px outer width): resize at <=85px from the right edge, move at >=88px (~41%).
  - Locals (456px outer width): resize at 150px (33%), move at 180px (39%).
- **Moves travel only about 2/3 of the scripted drag** (121px for 180, 134 for 200, 153 for
  228). xldb seems to apply the position from an earlier motion event, not the last. It may
  also coalesce motion or use XQueryPointer. With xdotool's discrete jumps this shows up as
  lag.
- A right press+release with **no motion does nothing** (neither move nor resize).
- A drag from the left third towards the right edge did nothing, possibly a rejected left-edge
  resize that would invert the pane.
- Early probes shrank Locals (516 -> 452) when released back at the press point after a detour,
  so resize applies with the same lag effect.
- **Unresolved:** the exact zone formula, whether the vertical zones match, and the
  corner behaviour. Scripted pointer jumps interact with xldb's motion handling, so the right
  tool is an X protocol trace (`xtrace` between xldb and Xvfb) to read the actual
  QueryPointer/ConfigureWindow traffic.

## Size from the Window Control menu (resolves design §12.5)
- Choosing `Size` puts the pane in a **sizing mode**. It keeps the white active border even
  when the pointer leaves it, and nothing is drawn while the pointer just moves.
- **Pressing** a button shows the XOR outline. Dragging moves the edge(s) on the side of the
  pane where the pointer is (pressed to the right of Locals -> right edge) **by the drag
  distance since the press** (+40px drag -> width 516 -> 556).
- **Releasing applies** the new size. This matches the help's "drag the cursor across the
  window edge(s) ... and click any mouse button".

---

# Recon pass 9: X protocol trace (xtrace) of move/resize and startup (`xtrace/*.log`)

Setup: `xtrace -d 127.0.0.1:42 -D 0.0.0.0:43 -n -k -o trace.log` on the host. xldb runs with
`DISPLAY=192.168.76.1:43`. Tools: `tools/tracedrag.py`, `tools/zoneprobe.py`. This supersedes
the pass 8 guesses about zones and "lag".

## Right-button press: move or resize, decided at press time
The pane's **inside** (excluding its 2px border) is split into a **3x3 grid by thirds** of its
width and height, using the press position relative to the pane (event-x/event-y):
- **centre cell -> move**. Horizontally, move if `w/3 <= x <= 2w/3` (Callers w=234: 78..156).
  Vertically, move if `h/3 < y <= 2h/3` (h=282: 95..188). The x and y tests differ by one pixel
  at the lower bound (`<=` vs `<`).
- **side cells -> resize that one edge** (middle-left = left edge only, even if the drag goes
  up).
- **corner cells -> resize both adjacent edges.**

## Protocol sequence
1. ButtonPress(3) -> `CreateGC` (first time) -> `QueryPointer` -> `GrabPointer` (owner-events
   false, confine-to = frame, pointer and keyboard async) -> `ChangeGC` (function=**Xor**,
   foreground=**0xa5a5a5**, subwindow-mode=**IncludeInferiors**, graphics-exposures=false).
2. Draws the outline as **one PolySegment of 4 segments on the frame window**, covering the
   pane's outer box: (x, y) to (x + w + 2*bw - 1, y + h + 2*bw - 1) in frame coordinates.
3. Each MotionNotify: one PolySegment of 8 segments (erase the old rectangle, draw the new).
4. ButtonRelease: erase (4 segments), `ConfigureWindow x y width height`,
   `ConfigureWindow stack-mode=Above`, `UngrabPointer`.

## Move mode
The outline position **lags one motion event**: on each MotionNotify it draws the rectangle for
the *previous* event's pointer position. Release applies the last drawn rectangle, so the
final move = drag distance minus the last motion step. With a real mouse this is a pixel or
two. With coarse scripted jumps it looked like "2/3 of the drag" (pass 8).

## Resize mode
While the pointer is still inside the pane on the resizing side, the outline doesn't change.
When the pointer reaches the edge, xldb **re-grabs the pointer** (a second GrabPointer). From
then on the edge follows the pointer: the right edge snapped exactly to the pointer (outer
right edge = release x). The left and top edges ended a few pixels short, consistent with the
same one-event lag. Final geometry via ConfigureWindow on release.

## Startup facts from the trace
- Frame: `CreateWindow ... x=33 y=73 width=957 height=846 border-width=2`,
  background-pixel `0x4876ff`, border-pixel `0x000000`.
- Panes: border-width 2. Chips are separate windows (e.g. Callers chip `204x12+528+86`).
- **Colours:** `AllocColor` with literal RGB for **`#4876ff`** (solves the RoyalBlue puzzle: xldb
  never uses the name), **`#fa1340`**, **`#d0d0d0`**, **`#5151fb`**. `AllocNamedColor` for
  **`Cyan`**, **`MediumBlue`**, **`LightBlue`**.
- **Fonts:** `OpenFont 'Rom14.500'` (fails), then `OpenFont '8x13'`.
- **Cursors:** a 16x16 bitmap cursor (white fg, black bg, hotspot 1,1: the pointer from pass 6).
  A second 16x16 bitmap cursor with hotspot **7,2** (probably the busy cursor). A glyph cursor
  from the `cursor` font, chars 0/1 (`X_cursor`).
- **Bitmaps:** several 16x16 and one **64x64** depth-1 pixmap (the icon) uploaded with
  PutImage. xtrace doesn't print image data. To get the exact bits, run a byte-level proxy
  (e.g. `socat -x TCP-LISTEN:6044,fork TCP:127.0.0.1:6042`) and decode the PutImage requests.

---

# Recon pass 10: exact bitmaps and horizontal scrollbars (`xtrace/bitmaps-decoded.txt`, `xtrace/session2-glyphs-hsb.txt`)

Setup: `tools/rawproxy.py 6045 127.0.0.1 6043 DIR` logs the raw client->server bytes and
forwards to the xtrace proxy on :43, which forwards to Xvfb :42. xldb runs with
`DISPLAY=192.168.76.1:45`. `tools/putimages.py DIR/conn-1` decodes every depth-1 PutImage
using the server's image format from the setup reply (Xvfb: LSB byte and bit order, 32-bit
unit and pad). Scenario: `rich`, breakpoints on lines 33 and 34, run to 33, disable 34,
horizontal scrollbar on Locals, open Help.

## All bitmaps xldb uploads (complete list for these scenarios)
| Pixmap | Size | Use |
|---|---|---|
| `0x200005` / `0x200007` | 16x16 XYPixmap | pointer cursor source / mask (hotspot 1,1, white fg, black bg) |
| `0x20000a` | 16x16 Bitmap -> depth 24 | frame tile (white on `#4876ff`), same as pass 9 |
| `0x200022` | 16x16 Bitmap -> depth 24 | scrollbar trough tile (1x1 checkerboard, white at 0,0) |
| `0x2000a1` / `0x2000a3` | 16x16 XYPixmap | **busy cursor = stopwatch**, hotspot (7,2). **Mask == source**, so it's white only, with no outline |
| `0x2000a6` | 64x64 XYPixmap | **icon = the bug**, set via `WM_HINTS` (IconPixmapHint) |
| `0x2000aa` / `0x2000a8` | 32x16 Bitmap -> depth 24 | execution arrow: outer shape / inner shape |
| `0x2000b2` / `0x2000ae` / `0x2000b0` | 11x11 | stop sign: outer octagon / inner octagon (clear) / inner octagon (fill `#fa1340`) |
| `0x2000c9` / `0x2000c5` / `0x2000c7` | 11x11 | disabled stop sign: same shapes, fill `#4876ff` |

The exact bits are in `xtrace/bitmaps-decoded.txt` (`#` = 1). Both tiles match what dbxl
already uses.

## How glyphs are drawn: plane-mask compositing (explains `#000001`)
Each glyph layer is a depth-24 pixmap made from a bitmap (fg 0xffffffff, or the fill colour,
on bg 0), copied onto the pane with CopyArea through one of three GCs:
- `AndInverted`: clears the shape to 0 (black),
- `Or` with `plane-mask=0x00000001`: sets bit 0 in the shape, making `#000001`,
- `Or`: ORs the fill colour in (onto the just-cleared pixels, so it gives exactly the fill).

| Glyph | Sequence | Result |
|---|---|---|
| arrow | outer AndInv, outer Or(pm 1), inner AndInv | `#000001` ring, `#000000` interior |
| stop sign | outer AndInv, outer Or(pm 1), inner AndInv, inner-fill Or | `#000001` ring, `#fa1340` interior |
| disabled stop | outer AndInv, outer Or(pm 1), inner AndInv, inner-fill Or (`#4876ff`) | `#000001` ring, background interior |

**Placement** (pane coordinates): arrow at (2, line_top) with line_top = 13 + 13*row, 32x16.
Stop signs at (2, line_top + 1), 11x11. When both are on a line, the stop sign is drawn first
and the arrow over it.

In the mono schemes, the fill pixmaps are made with the scheme's "red"/background pixel,
which is black. That's the pass 7 behaviour, which dbxl deliberately changes (DESIGN.md §11).

## Move/resize cursors (cursor font, fore black / back white)
Created together on the first right-press:
`fleur` (0x34) for the initial grab. On reaching an edge, xldb re-grabs with the edge cursor:
`top_side` 0x8a, `bottom_side` 0x10, `left_side` 0x46, `right_side` 0x60,
`top_left_corner` 0x86, `top_right_corner` 0x88, `bottom_left_corner` 0x0c,
`bottom_right_corner` 0x0e. (Traced: move -> fleur, right edge -> right_side, top-left corner
-> top_left_corner, left side -> left_side.) An `X_cursor` (char 0) is created at startup but
never used.

## Busy cursor
The frame's cursor switches to the stopwatch while xldb works (startup symbol loading,
execution commands) and back to the pointer when it's done:
`ChangeWindowAttributes window=frame cursor=busy` ... `cursor=pointer`.

## Horizontal scrollbars (resolves the milestone 1 open item)
- A horizontal bar is at `(-2, h - 11)`, `w x 9`, border 2. Its arrows (9x9) are at `(0,0)` and
  `(w - 9, 0)`. This mirrors the vertical bar, as dbxl assumed.
- Arrow polygons: left `(8,0),(0,4),(8,8),(8,0)`; right `(0,0),(8,4),(0,8),(0,0)`. dbxl's
  right-arrow vertices start from a different point and should be copied exactly.
- Thumb: `(13,-1),(25,-1),(25,28),(13,28),(13,-1)`, the same as dbxl's.
- With both bars (Help), **neither bar is shortened**: vertical `589,-2 9x750`, horizontal
  `-2,739 600x9`. They overlap in the bottom-right corner.
- **Creation order: vertical bar first, then horizontal**, so the horizontal bar is on top in
  the corner. dbxl currently creates them the other way round.

---

# Recon pass 11: the milestone 2 UI scenario (`m2ref/`, `xtrace/m2-scenario.txt`)

`dbxl-debugger/tests/visual/scenario.py` drove xldb (`bsh`, default layout) through 33 steps
with a capture after each (`m2ref/m2-*.png`), and the X traffic was traced. The same script
drives dbxl in `make visual`.

## Cell menus (Window Control, Options, Display controls, Select Breakpoint Action)
- A frame child with a 2px black border and the title colour as background, `w x (17 + 19n)`.
  Each item is a `w x 17` child with a 2px black border at `(-2, 17 + 19k)`. Item text is
  centred at baseline 11 on the pane background, or black (highlight) with white text.
- Title: centred in a `(w-8)/8`-column field at (4, 11), rounding the left pad up.
- Width: Window Control = its widest item (176). Display controls = widest item (264).
  **Options = 288**, wider than any visible item (probably sized for unseen values).
- Options items are pre-formatted: `" label" + spaces + "value "` with label + gap + value =
  30 columns and at least 4 spaces (`" Breakpoint all Subprograms    no "` is longer).
  Display controls use 31 columns.
- **Placement:** Window Control at the pane's outer corner. Options at `(anchor_x - ?, anchor_y
  - 10)` clamped to the frame; both recorded placements were clamped against the right edge,
  so the x offset is unknown (dbxl uses -10). Display controls reuses the Options anchor.
- **Pointer:** opening warps it to `(w/2 - 4, 4)` inside the default item. The highlight
  follows the pointer (Enter/Leave). Selecting warps it back to the anchor (the click that
  opened the menu).
- **The menu closes when the pointer leaves it** (LeaveNotify on the menu window). **Escape
  does nothing.** (This explains pass 1's "click outside closes it and the click goes
  through".)
- **Toggles** (Options values, Display controls) redraw the item and **keep the menu open**.
  Register control replaces Options with Display controls at the same anchor.
- xldb saves and restores the frame area under a menu itself (CopyArea to a pixmap).

## Dialogs
- **Confirm** (Exit): 400x60 centred in the frame (278,393), `#0000cd` with a 2px white border.
  Buttons `yes`/`no` 64x13 with a 1px white border at `(w-75, 9)` and `(w-75, 36)`. Message
  at baseline 34, x = `(w - 85 - len*8 + 1)/2` (98). A hidden field is filled with garbage (an
  xldb bug).
- **Prompt** (Breakpoint "Enter function name", Save Window "Enter the target file name",
  Edit source search path): frame width x 60 at `(0, frame_h/2)` = (0,423). Buttons
  `proceed`/`cancel` at the same offsets. Field `(w-96) x 13` at (9,36), LightBlue with a 1px
  white border. The message is centred **by character columns** left of the buttons:
  `x = 8 * ((cols - len)/2)` with `cols = (w-85)/8`, at baseline 21.
- Field text: black, a padded string at (0,11). Cursor: `PolyRectangle` 8x12 in `#fa1340` at
  `x = 8 * cursor`.
- Opening warps the pointer onto the first button at (61,10). Closing warps it back to the
  anchor.

## Inline strip
`:` (and `/`, `\`, `?`) with the pointer in a pane creates a `w x 13` child of the pane with a
1px white border at `(-1, pointer_y + 18)`. It's LightBlue with black text and the red cursor
box. Escape destroys it.

## Messages bar
The Messages window is a pane 947x12 at `+3+826` whose **title bar is the message**
(centred like any title). It's mapped when a message arrives and **unmapped at the next
button press in a pane** (not a chip). A click in an empty data pane (Globals/Monitor)
gives `Click on a data object.`

## Window Control actions
- **Maximize** = `x=2 y=2 949x838`: the frame less a 2px margin (pass 5's "whole frame" was
  slightly wrong). Restore returns to the old geometry.
- Minimize maps the pane's chip at its IconGeometry. Chips never re-flow.
- Horizontal and Vertical scroll bars toggle the bars (mirrored geometry, pass 10).

## Tags (tagWindows)
Tags: Locals L, Source So, Callers Ca, Commands Co, Breakpoints B, Globals G, Monitor M,
Files F, Subprograms Su, Threads Th, Storage St, Help H. Disassembly, Registers, Messages:
none. The tag is drawn at x=0 and at `w - len*8 - 11` with a vertical scrollbar
(`- 1` without). **Tags that would overlap the title are left out** (the messy layout's
74px Commands pane shows only "Commands"). Chips never show tags.

## Mono schemes (correction to pass 8)
Scroll bar arrows **are filled** with the foreground colour in `-bw`/`-wb` (black / white).
The checkerboard trough made them look hollow in pass 8.

## Resources
- The messy sample (`-name messy-xldb`) applies window, icon and scrollbar geometries, and a
  commandList without keys (no Alt column).
- **`HelpIconStartup: true` does not show a Help chip at startup.** Help stays hidden.

# Recon pass 12: running a program (milestone 3)

Setup: `xldb ./tests` (test.c) driven by `dbxl-debugger/tests/visual/scenario_m3.py
--xldb`, captures in `m3ref/`. Then `xldb ./rich` for scrolling. Message texts come from
the catalogue compiled into `/usr/lpp/xldb/bin/xldb` (`strings | grep '^M_'`), saved as
`xldb-messages.txt`.

## Stops
- On **every** stop the Source view is re-centred: top line = stop line - 15 (half of the 31
  rows), at least line 1, **even when the line was already visible** and even when that leaves
  blank rows below the end of the file (top can exceed the last full page).
- The arrow is drawn over a stop sign on the same line (stop sign first).
- Callers lists `func()` per frame with the current frame in cyan. At the start-up stop and
  the first breakpoint only `main()` is listed; later stops add `__start()`.
- A left click in Source (setting or clearing a breakpoint) drops the Callers highlight.
- Locals is titled `Locals for main() in test.c`.
- Stopped in code without source (after a SIGSEGV in libc `kfcntl`): Source is titled with the
  assembler file name (`glink.s`), is empty, and has the arrow on the first row. Locals is
  `Locals for kfcntl() in glink.s`.

## Messages (from the catalogue; the first four were seen)
- `Breakpoint encountered.` (a user breakpoint; nothing for the start-up stop or steps)
- `3 breakpoints set` / `3 breakpoints removed` for line 17, which has three addresses on
  POWER. Singular: `Breakpoint set`, `Breakpoint removed`.
- `Program stopped during trace by signal 11: SIGSEGV (segmentation violation).`
  (`19: SIGCONT (continue)` for the emulator's first-Continue quirk).
- `Program exited with return code %d.`, `Program terminated by signal %s.`
- `Cannot breakpoint line %d of `%s'`,
  `Subprogram '%s' not found (perhaps it was not compiled with -g).`,
  `The source file was not found in the source search path.`
- The Breakpoint dialog (`add`) sets a breakpoint at the function's **`{` line** (entry,
  before the prologue) and shows **no message**. Restart shows none either.

## Stepping on AIX (not reproducible with GDB)
- Step at line 17 (breakpoint, `total = add(total, i)`) stays on line 17; Next then goes to 18.
- Return in `main` runs until the breakpoint on 17 is hit again.
- Every run ends with SIGSEGV in `kfcntl` during `exit()` under the emulator; Continue
  repeats the same signal stop forever, so normal exit could not be observed.
- Machine step at the `{` of `add` stays on line 4.

## Stop sign drawn on click
When a breakpoint is set by clicking, the new stop sign's outline is drawn twice, at
line_top and line_top+1, without clearing, so its top looks doubled (rows 0-3). A full redraw
(m3-12, set from the dialog) gives the normal 11x11 sign at line_top+1.

## Scroll thumb (vertical)
The thumb is always 12px (outline at `pos` and `pos + 12`). With the pane `h` tall (420 for
Source), rows = (h-13)/13 = 31, and top the 0-based first visible line:
```
range = max(1, nlines - rows)
pos   = 13 + min(h - 38, (h - 39) * top / range)        (integer division)
```
Checked at every position of `rich.c` (49 lines: top 0..23 gives 13, 34, 55, ... 372, 394,
then 395 for top >= 19) and `test.c` (21 lines: top 1 -> 394, top 2 -> 395).
The arrows scroll by one line: up while top > 0, down only while top < nlines - rows (so a
view centred past the last page doesn't move down, and a file shorter than the pane never
scrolls).

# Recon pass 13: data panes and variable menus (milestone 4)

Setup: `xldb ./rich` through the raw proxy and xtrace (`xtrace/m4-session.txt`). Driven with
`tools/rec.py` (every action logged in `m4ref/actions.jsonl`, captures in `m4ref/`). Pane text
was read from the trace with `tools/panetext.py`, which replays the PolyText8 requests, instead
of from screenshots. The session includes exploratory misclicks, so the milestone 4 test
scenario should be re-recorded as a clean script against a fresh xldb.

## Variable menu = a pane
- It's one window, created at startup: a frame child **201x460 with a 2px border** (the pass 3
  "1px" was wrong) at frame (336,141), with the two scrollbar children every pane has (not
  mapped). It's drawn exactly like a pane: a 12px title bar with the title centred (`pointer`
  at x=72), rows as 25-column padded strings at baselines 24+13n, section headers
  (`Detail:`, `Style:`) in menuInfo grey, and the highlighted row as a 13px cyan bar with
  select-foreground text.
- The title is the type: `int`, `char`, `short`, `long`, `unsigned-char`, `float`, `double`,
  `pointer`, `array`, `structure` (multi-word types are hyphenated).
- **Opening:** save the frame area under it (CopyArea into a 205x464 pixmap), raise and map it,
  and warp the pointer to (4,32) in the menu: row 1, the first item under the header.
- **Border:** white while the pointer is inside, black after it leaves, like a pane. The menu
  **stays open** when the pointer leaves.
- **Closing:** choosing an item (unmap, warp the pointer back to the click that opened it, copy
  the saved area back, then panes redraw), or a click elsewhere (on frame background, at
  least). Clicking a section header does nothing, and the menu stays open.
- It's resizable and movable like a pane (a right-drag on it resized it). Its geometry then
  sticks, and the Edit / trigger dialogs open at the menu window's current position.
- **Highlighted item:** the last item chosen, if this menu has it. Otherwise it's the last item
  chosen in a menu of the same kind (`int`, `char`, `short`, `long` and `unsigned-char` share
  one list). Initially `more` for pointers/arrays, `flatten` for structs, and (pass 3)
  `signed` for integers.

Item lists (each preceded by its headers):
- integers (`int char short long unsigned-char`): `Style:` character, signed, unsigned, hex,
  address, type, size, save, recall, default; then Edit, Breakpoint, Monitor on/off, Function
  parameter, Storage view.
- `float`/`double`: `Style:` decimal, scientific, hex, address, type, size, save, recall,
  default; then Edit, Breakpoint, Monitor on/off, Function parameter, Storage view.
- `pointer`: `Detail:` more, less; `Style:` hex, string, array, pointer, address, type, size,
  save, recall, default; then Breakpoint, Select subrange, Edit, Cast, Downcast, Monitor
  on/off, Function parameter, Storage view.
- `array`: `Detail:` more, less; `Style:` string, address, type, size, save, recall, default;
  then Select subrange, Monitor on/off, Function parameter, Storage view.
- `structure`: `Detail:` more, less, flatten; `Style:` address, type, size, save, recall,
  default; then Show self, Monitor on/off, Function parameter, Storage view.

## Styles (results)
| | int `a`=100 | char `tag`='b' | pointer `sp` | float `ratio` |
|---|---|---|---|---|
| character | `'d'` | `'b'` | | |
| signed | `+100` | `+98` | | |
| unsigned | `100` | `98` | | |
| hex | `0x00000064` (-5: `0xfffffffb`) | `0x62` | `0x20014a80` (the value) | `0x3e800000` (bits) |
| address | `@2ff22bec` | `@2ff22bf0` | `@2ff22be8` (of the variable) | `@20014ae8` |
| type | `int` | `char` | `-> shape-struct` | `float` |
| size | `<4>` | `<1>` | `<4>` | `<4>` |
| string | | | `"box"` | |
| array | | | `[]` | |
| decimal / scientific | | | | `0.25` / `2.5e-1` |
- Hex digits = the type's size: short `0xfffd`, unsigned char `0x81`, long `0x075bcd15`.
- `type` on arrays: `[0..3] point-struct`. On unsigned char: `unsigned-char`.
- `save` stores the current style, `recall` restores it (after `default`, say), and `default`
  resets the style but not the detail level.
- **Styles and detail levels are per variable and shared** by Locals, Globals and Monitor. They
  survive stepping and Restart.
- `more` on a pointer shown in another style switches it back to pointer style.
- Pointer `char *greeting`: `ptr`; `more` gives `->'h'`, `string` gives `"hello, world"`,
  `array` gives `[]`.

## Floating point
Significant digits: **6 for float, 15 for double** (`3.14159265358979e+0`,
`1.23456789012e+11`, `6.66666666666667e-1`).
- **scientific** (the default): mantissa with trailing zeros stripped but at least one decimal
  (`7.0e+0`, `1.0e-1`, `1.23457e+6`), an exponent with a sign and no padding (`e+38`,
  `e-300`), zero is `0.0`, and negatives have a leading `-`.
- **decimal:** fixed-point for decimal exponents **-5 to 6** (`0.00001`, `0.0001`, `0.1`,
  `123.456`, `999999.0`, `1000000.0`, `1234570.0`), with trailing zeros stripped but at least
  `.0`. Outside that range it uses the scientific form (`1.0e+7`, `1.23457e+7`, `1.0e-6`).

## Detail levels (`more` / `less` / `flatten`)
- pointer: `ptr` (`<null>` if 0) -> `->shape{}` -> `->{ [] GREEN point{} ptr 4 1.5e+0 }`.
  `less` steps back, never below `ptr`.
- struct: `shape{}` -> `{ [] GREEN point{} ptr 4 1.5e+0 }` (inline, no field names) ->
  **vertical with field names**, one member per line, aligned after `name: {`:
  ```
  sp: ->{ name: [ [ 0]: 'b'
                  ...
                  [11]: '\0' ]
          color: GREEN
          origin: { +5 +5 }
          corners: ->point{}
          ncorners: 4
          scale: 1.5e+0 }
  ```
- array: `[]` -> inline `[ -5 +2 +3 +4 +5 ]` -> vertical `[ 0]: -5` rows aligned after
  `name: [ `, closing ` ]` on the last element. Struct elements in a vertical array show one
  level expanded (`[ 1]: { +10 +0 }`); in an inline array collapsed (`point{}`).
- `flatten` expands everything inline (not through pointers):
  `box: { [ 'b' 'o' 'x' '\0' ... ] GREEN { +5 +5 } ptr 4 1.5e+0 }`.
- **Wrapping:** inline arrays break after **10 elements**, and the continuation is indented to
  the column after the opening `[ ` (+2 for `->{ [`...). This doesn't depend on pane width: a
  35-column pane just cuts lines off at the right edge, with no rewrap. Every row is drawn as a
  full-width padded string.
- Each element of an expanded value is clickable and gets its own menu.

## Panes
- Locals: `name: value` rows in declaration order (parameters first).
- Globals: a blank row, `----- File rich.c -----`, a blank row, then the file's globals in
  declaration order.
- Monitor: `----- Globals -----`, the monitored globals, a blank row, `----- Locals -----`,
  the monitored locals. Entries are in declaration order (not the order they were added), and
  use the shared style and detail state. `Monitor on/off` toggles.

## Edit dialog
The prompt dialog at **400x60 at the menu window's position** (frame (336,141) by default),
with a 2px white border and title-blue background. The message is centred like the confirm
dialog, `(w-85-len*8+1)/2` = x 94, at baseline 21. The field is 304x13 at (9,36) and prefilled
with the value as displayed (`+100`), with the cursor box after it. The buttons are 64x13 at
(325,9) `proceed` and (325,36) `cancel`. The pointer warps to (61,10) on `proceed`. Return
applies (the value accepts C syntax: `-5`, `1e-10`, `0.3333333`).
`Breakpoint` in a variable menu opens the same dialog with `Enter breakpoint trigger:` (a
conditional breakpoint, milestone 5).

## Callers frame selection
Clicking a frame moves the cyan highlight in Callers to it. Locals switches to that frame
(`Locals for main() in rich.c`), and Source re-centres on the frame's line and draws it as a
**full-width cyan bar**. The arrow stays at the stop point. Selecting the top frame gives the
cyan bar on the stop line, under the stop sign and arrow. (xldb showed zeros for `main`'s
locals while stopped in `area`, likely an emulator/xldb frame-reading problem. Don't
replicate.)

## Not explored
Select subrange, Cast, Downcast, Show self, Function parameter; Storage view is in pass 5.

## Corrections from the milestone 4 scenario (`m4scen/`, same session tools)
- **Detail levels are relative.** A child shows at (parent's level - 1) + (its own level -
  1), at least 1. With `name` given one `more` while `sp` showed its target inline (`name`
  inline, wrapped), a further `more` on `sp` made the target vertical **and** `name` vertical
  (`[ 0]: 'b'` rows). This matches the help: "If the components of the structure object are
  already displayed when 'more' is selected, then 'more' will also be applied to each of the
  components."
- **`flatten` = `more`, kept horizontal.** From collapsed, `box: shape{}` became
  `box: { [] GREEN point{} ptr 4 1.5e+0 }` (one level, like more). From inline it gave the
  fully expanded one-line form of pass 13. Pointers inside aren't followed (`ptr`). The help
  says: "To show the members of a structure object horizontally ... click flatten."
- **Edit checks the text against the style:** with `a` shown in hex, entering `-5` closed the
  dialog and immediately opened a new `Enter new value:` prompt (rejected). In signed style
  `-5` was accepted.
- The extra signal stop on the emulator (SIGCONT/SIGHUP) happens only on the first run. After
  Restart, the first Continue went straight to the next breakpoint.

# Recon pass 14: breakpoints and machine-level panes (milestone 5a)

Setup: `xldb ./rich` through the raw proxy and xtrace (`xtrace/m5-session.txt`, bitmaps in
`xtrace/m5-putimages.txt`). Driven with `tools/rec.py` (`m5ref/actions.jsonl`, captures in
`m5ref/`).

## Breakpoints window
- Entries: `main [line 44 in rich.c]` (function, line, file), in the order set. A
  conditional one appends `  a: +100` (object and trigger). A disabled one appends
  `  Disabled`. Setting from Source says `Breakpoint set` (the count form for several
  addresses, pass 12).
- **Clicking an entry:** the entry gets a cyan bar. Source re-centres on its line with a
  full-width cyan bar. Then the cell menu **`Select Breakpoint Action`** opens with its outer
  corner at **the Breakpoints pane's centre** (frame (363+588/2, 200+160/2) = (657,280)), and
  the pointer is warped onto the first item. Items: `Clear breakpoint`, `Disable breakpoint`,
  `Enable breakpoint`, `Clear all breakpoints`, `Disable all breakpoints`, `Enable all
  breakpoints`. The first item is the highlighted default.
- **Disable / Enable / Disable all / Enable all / Clear:** no message. The list updates
  (`  Disabled` suffix), Source shows the hollow stop sign, and the cyan bars go away.
- **Clear all breakpoints** asks first: the confirm dialog (400x60, centred) **`Clear ALL
  user breakpoints?`** with `yes`/`no`. The entry and Source keep their cyan bars while it is
  open. `yes` empties the list, again with no message.

## Conditional breakpoints (triggers)
- Variable (or register) menu -> `Breakpoint`: the compact prompt (400x60 at the menu's
  position) **`Enter breakpoint trigger:`**, prefilled with the displayed value (`+100`).
- The trigger goes on **the current execution line**. An existing breakpoint on that line
  becomes the conditional one (no second entry). Entry: `main [line 47 in rich.c]  a: +100`.
- Trigger syntax (help): `X`, `!X`, `X..Y`, `!X..Y`, read in the object's display style
  (signed comparison for character/signed/decimal/scientific styles, unsigned otherwise).
- **Glyph:** the red stop sign with a `?` in the outline colour (`#000001`), drawn with the
  outer ring bitmap plus this inner fill:
  ```
  ...........
  ...#####...
  ..##...##..
  .##..#..##.
  .##.###.##.
  .####...##.
  .####.####.
  .#########.
  ..###.###..
  ...#####...
  ...........
  ```
  (the red interior is this mask; the `?` is where it is clear and the ring colour shows).
- When it triggers: `Conditional breakpoint (condition is satisfied).`

## Disassembly
- Title: the function's name (`main`). Default geometry 567x440 (frame (288,377)).
- Rows: `0x%08x (+0x%04x)  %-8s %s` (address, offset, mnemonic padded to 8, operands),
  except the first offset is written `(+000000)`. The whole function is listed from row 0.
- Margin glyphs as in Source: the arrow on the current instruction, stop signs (and the `?`
  sign) on breakpointed instructions.
- **Clicking an instruction** sets a breakpoint at that address: `Breakpoint set`. It shows in
  Disassembly and **on its source line in Source**, and is listed by source line
  (`main [line 45 in rich.c]`). It also drops the Callers highlight, like a Source click.
- **Machine step** moves the Disassembly arrow one instruction. The Source arrow follows the
  source line of the new pc (and Source re-centres as on any stop).

## Registers
- Title `Registers`, rows `%-5s: 0x%08x` (`IAR  : 0x100471ac`, `FPSCR: 0x00000000`,
  `GPR10: 0x20014ff4`), with no scrollbar in the default layout.
- Clicking a register opens the variable menu titled **`intregister`**: `Style:` character,
  signed, unsigned, hex, type, size, save, recall, default; then Edit, Breakpoint, Storage
  view (no address, Monitor or Function parameter). Default style hex; `signed` gives
  `GPR3 : +100`, `size` gives `<4>`.

## Storage
- Title **`Storage Pane`**. It starts at the data segment (`20000410:`). Rows:
  `%08X:  %08X %08X %08X %08X  ` then 16 characters, unprintable bytes as `\372` (`ú`).
- **Clicking a word** opens a cell menu titled `Storage` (items `Edit`, `Breakpoint`,
  `Storage view`) with its outer corner on the clicked word's cell: x = the word's first
  column - 2, y = the row's top - 2 (frame (92,429) for the word at column 11 of row 3). The
  pointer is warped to the first item.
- **Edit:** an in-place 64x13 LightBlue field with a 1px white border exactly over the word,
  its text the 8 hex digits, the cursor box on the **first** digit. The pane title becomes
  `Change Block at Address: 20000440`. Escape (with the pointer over the field) removes the
  field. xldb leaves the changed title (stale).

## Threads, Files, Subprograms
- Threads: after the header and `    All Threads`, one row per thread with the current one in
  cyan: `                         8815   user                       Breakpoint-05   Enabled`
  (thread id right-aligned to column 29, mode, reason under SIGNAL, state).
- Files: `rich.c` (basename). **Clicking** highlights it (and drops the Subprograms
  highlight) and shows the file in Source **from line 1, with line 1 in cyan**.
- Subprograms: `area()`, `main()`, sorted. **Clicking** highlights it, shows the function in
  Source centred on its `{` line in cyan, and shows its disassembly with the first instruction
  in cyan.

## Corrections from the milestone 5a scenario (`m5scen/`)
- Cell menus have their own widths in xldb: Window Control 176, Select Breakpoint Action
  200, Storage 112 (from `ConfigureWindow width=`), not one rule.
- Choosing an item in the Select Breakpoint Action menu warps the pointer back to the click
  (the pane keeps its white border), except `Clear all`, whose confirm dialog warps it.
- Dialogs warp the pointer back to their anchor **before** destroying their windows, so no
  pane under the dialog gets an Enter (and autoraises).
- A stop raises Source and then an open Disassembly. Selecting in Files, Subprograms or
  Breakpoints changes what Source shows without raising it.
- Setting a trigger, and selecting in Subprograms or Files, drop the Callers highlight.
- After a trigger is set, Source shows top line 20 (the current line 47 on row 27).
  Reproduced in two runs; the rule is unknown.

# Recon pass 15: milestone 5b (Options side effects, remaining menu actions, Help)

Setup: `xldb ./rich` through xtrace (`xtrace/m6-session.txt`), `tools/rec.py`
(`m6ref/actions.jsonl`, captures in `m6ref/`). Some steps went astray because panes opened
earlier covered the Commands pane. Those are noted, not used.

## "Formats" is the variable menu
xldb's help (Configuring layouts): the window name `Formats` "identifies the menu which appears
when an object is left-clicked in the Globals or Locals window". There is no other Formats
window: its resources are `FormatsGeometry` (201x460+336+141) and `FormatsIconGeometry`.

## Options menu items with effects
- **Save Breakpoints:** a full-width prompt **`save breakpoints file name`** (lower case),
  prefilled with the `breakpointsFile` resource or **`.xldb.<program basename>`**
  (`.xldb.rich`), or the name used last time. Proceed writes the file, with no message. The
  format is one line per breakpoint: `break <file> <file> <line> <the source line's text>`,
  for example `break rich.c rich.c 44     sp = &box;` (one space, then the line with its
  indentation).
- **Load Breakpoints:** the same prompt, **also titled `save breakpoints file name`** (an
  xldb slip), same default. Loading adds breakpoints, skips ones already set, no message.
- **Save layout:** a full-width prompt **`layout file name`**, empty. An empty name gives
  `Unable to save window layout in file .`; otherwise `Window layout saved in file <name>.`
  The file (written relative to the cwd):
  ```
  xldb.AutoRaise:on
  xldb.FunctionList:With -g  only
  xldb.specialLayout:true
  xldb.geometry:957x846+33+73
  xldb.<W>Geometry:<w>x<h>+<x>+<y>
  xldb.<W>IconGeometry:<w>x<h>+<x>+<y>
  xldb.<W>IconStartup:true|false        (currently minimized)
  xldb.<W>Scrollbars:vertical|horizontal|both|none
  ```
  Windows in order: Callers, Files, Subprograms, Breakpoints, Commands, Source, Locals,
  Globals, Monitor, Formats, Messages, Help, Disassembly, Registers, Storage, Threads.
  Formats and Messages have only Geometry and IconGeometry. Help writes IconStartup false even
  while hidden. The geometries are the current ones.
- **Detail per click:** **left-click decreases, right-click increases** (minimum 1, as the
  help says). With 2, `more` on `ptr` gives `->{ [] GREEN point{} ptr 4 1.5e+0 }` and `less`
  goes back to `ptr`, but only because that `sp` had been stepped before: the first `more`
  on any object goes to detail 2 whatever the step (pass 16).
- **Subprograms: All** lists every function as `%08x:   name()` (address, colon, 3 spaces),
  in address order. The menu value is left-aligned in a 13-column field (`All` followed by
  spaces, like `With -g  only`).
- **Breakpoint all Subprograms: yes** immediately sets a breakpoint on the `{` line of every
  function with debug info (`area [line 27]`, `main [line 39]`), with no message.
- **Case Sensitive** affects searches only (help). **Local variables: All** shows variables
  of inactive blocks too (help); `rich` has no nested blocks.
- A breakpoint set in a function without source is listed as `__modfini [line 1 in
  __modinit.c]`.
- Closing the Select Breakpoint Action menu without choosing (pointer leaves) keeps the cyan
  bars on the entry and in Source.

## Edit input is checked against the style
In hex style, `-5` is rejected: the dialog closes and a new `Enter new value:` prompt opens
with the entered text kept, and Messages shows the style's hint:
`Enter an unsigned hex number (examples: 0x12,  5a, 13, deadbeef, 0x5465).` The hints and
range errors for every style are in `xldb-messages.txt`: `M_*_input_help`,
`M_<n>_byte_<style>_integer_out_of_range` (`Enter a value in %i .. %i.`),
`M_invalid_floating_point_number`, `M_invalid_hex_digit`. Accepted syntax per style is in the
help's "Changing ..." sections.

## Remaining variable-menu actions
- **Select subrange** (pointer): the compact prompt **`Specify array subrange(s)`**,
  prefilled **`[0..0]`** with the cursor at the start. `[0..2]` shows the pointer as an
  inline array of those elements: `c: [ point{} point{} point{} ]`. Hints:
  `M_subscript_or_range_expected`, `M_specify_a_range_in_brackets`, and (strings)
  `M_character_string_subrange_help`.
- **Cast** (pointer): the Formats window titled **`Select new base type`**, listing the
  program's struct/union/enum tags and typedefs (`point`, `point_t`, `color`, `shape`), with
  no header rows, the first one highlighted. Choosing one shows the pointer as
  `c: ( -> shape-struct )`.
- **Function parameter:** message `Address of selected object will be argument for next user
  command invocation` (for commandList `sub()`, milestone 6).
- **Downcast** and **Show self** do nothing on C objects (C++ only).
- **save / recall:** `save` shows `Use "recall" to apply the saved style to an object.` The
  saved style is **one global clipboard**: `recall` on another object applies it, and without
  a saved style says `Use "save" to save a style before using "recall".`

## The Source scroll after a trigger (superseded by pass 16)
After a trigger is set, Source scrolls so the current line sits **4 rows above the bottom**:
top = max(0, line-1 - (rows-4)) (0-based). Line 47 gives top line 20 (m5-13); line 27 gives top
line 1.

## Help
- Opens at the help's index (`Help index`, `----------`, a blank line, then `  --> N. title`
  lines), titled **`Help [exit][index][return]`** (centred, x=196 in the 600-wide pane).
- Clicking a `-->` line scrolls so that section's heading is the top row. `[return]` goes
  back, `[index]` shows the index, `[exit]` closes Help.
- The index ends with `Left-click any line starting with "-->" to see the corresponding
  text.`, `You can save a copy of the help text by left-clicking the line below:` and
  `    --> copy help text to $HOME/xldb.help.text`. That link writes the file and says
  `The help text was written to file /home/tester/xldb.help.text.`

# Recon pass 16: the milestone 5b scenario (`m6scen/`)
Recorded `tests/visual/scenario_m6.py` against xldb (`--xldb`) and compared with dbxl.
Corrections:
- **The first `more` (or presumably `flatten`) on an object goes to detail 2**, whatever
  Detail per click is (tried with 1, 2 and 3 on fresh `sp`, `box`, `square`, `s`). After
  that, `more`/`less` step by Detail per click (`box`: more → 2, less → 1, more → 3; `s` with
  3: more → 2, less → 1, more → 4).
- **Source scroll after a trigger:** the current line sits **5 rows above the bottom**
  (row rows-5), and Source does not scroll past the end of the file:
  top = clamp(line-1 - (rows-5), 0, nlines - rows). Line 45 → top line 19; line 47 → top
  line 19 (clamped, 49 lines, 31 rows); line 27 → top line 1.
- **A trigger on a line without a breakpoint** says `Breakpoint set` (it creates one). On a
  line that already has one (m5-13, pass 15's `area` case) there is no message.
- **Full-width prompt titles** are centred by pixels over the 872 px left of the buttons:
  `layout file name` at x=372, `save breakpoints file name` at 332, `Enter function name`
  at 360 (whole-column centring gave 368 for the first).
- **After an Options dialog closes** the pointer is warped back to where the menu item was
  chosen (frame (810,462) for Save Breakpoints), not to the Options command.

# Recon pass 17: milestone 6 (signals, termination, options, commandList calls)
Captures in `recon/p17/` (actions in `actions.jsonl`), trace in the session xtrace. Launch
helper: `tools/recon/xl.sh ARGS` (kills xldb, starts `xldb ARGS` in ~ on :45).

## Signals and the Signal command
- A signal sent while the program is stopped (kill -SEGV from a shell) shows nothing until the
  program runs: Continue then stops at once with `Program stopped during trace by signal 11:
  SIGSEGV (segmentation violation).` at the same line.
- **Signal** (no dialog) resumes and passes the signal that stopped the program: here the
  program dies. With no pending signal it acts like Continue. Continue/Next/Step/Return
  *discard* the stop signal (help). GDB's `-exec-continue` passes it, so dbxl must discard
  explicitly (`signal 0`) and use `signal SIG` for Signal.
- `-i segv -i 19` (names or numbers): those signals go straight to the program without a stop
  (19 also removes the emulator's first-run SIGCONT stop).

## Program termination
- A program killed by a signal (Signal on SIGSEGV, or `-i segv`) gives **`"./rich"
  terminated. Termination code is: -1.`** (M_program_terminated_with_return_code), not
  `Program terminated by signal %s.` A core file is written.
- The terminated state clears **everything**: Locals and Source titles become empty (blank
  title bars), Locals, Callers and Source are empty, Storage is empty with title `Storage`.
  Globals opened afterwards first shows garbage (every byte 0xff: `flags: 255`,
  `ratio: -NaNQ`), then empties on the next refresh (xldb artifact; dbxl shows it empty).
- Continue, Next, Step, Machine step, Return: each shows `Can't continue unless at least one
  thread with a pending signal is Enabled` (not in xldb's catalogue: from the debug library).
  Signal shows `Running...` first, then the same.
- Breakpoint command: the prompt opens; `area` is accepted silently but no breakpoint is set;
  the message goes back to the termination text.
- Subprograms still lists functions; clicking one shows its source with the cyan bar (title
  `./rich.c`). Clicking a Source line then re-shows the terminated state (Source blank again,
  termination message), no breakpoint.
- **Restart after a signal death fails in xldb** (no process; `Function source is not
  available. Use Disassembly pane or recompile with -g` and an empty Disassembly window pops
  up). Treated as an xldb/emulator bug: dbxl restarts normally.
- Normal exit still cannot be observed (the emulator crashes every exit in `kfcntl`).

## Core files (`-co`)
- With AIX's defaults xldb can't read cores: light cores give `xldb: Read failed for corefile:
  errno=2` (exit 1); with `fullcore=true` but the 4.3 layout, `Error: executable "rich"
  specified, but core file was generated by executable "ÿÿÿÿ"` (exit 1). It works with
  **`fullcore=true` and `pre430core=true`** on sys0 (set by the user as root, 2026-09-28).
  Core made with dbx: `stop in area`, `run`, `cont SIGSEGV` (dies in `area` at line 27).
  xldb -co still starts `./rich` as a child process.
- **`xldb -co ./rich`:** a normal stopped display at the failing line (Source centred on
  `area`'s `{` with the arrow, `Locals for area() in rich.c` with the core's values).
  Message: **`Program terminated by signal 11: SIGSEGV (segmentation violation).`**
- **Commands** has only `Edit`, `Exit`, `Options`, `Help`, **without Alt keys** (rows padded
  to 25 columns like the others).
- **Callers:** `area()`, `main()`, `0x00000000`, `__start()` (an unnamed frame shows its
  address).
- Allowed: Callers frame selection (cyan bar, Locals switch), Globals (values from the core),
  Storage (the core's data), Disassembly (all words `00000000`: xldb reads text from the core,
  which has none), **breakpoints** from Source and Disassembly (`Breakpoint set`, stop sign
  drawn; `Breakpoints not allowed with core files` never appeared).
- Refused: variable **Edit** says `Modification not allowed with core files` at once, with no
  dialog. **Storage** in-place edit opens (title `Change Block at Address: 20000420`), and
  Return says `Storage modification not allowed with core files`; the value is unchanged and
  the title stays.

## Attach (`-a`)
- Every attach fails before a window appears: `Unable to find member shr4.o of
  /usr/lib/libiconv.a.xldb: Unexpected error: reading symbol table ... errno=22` (exit 1),
  even for `bsh` and with `-n`. A failed attach killed the target process. Not observable here.

## `-q`, `-r`, `-c`, `-e`
- **`-q`:** no window at start; the program runs from the beginning without stopping at
  `main`, and the window appears at the first signal stop (a normal signal-stop display).
- **`-r area`:** the first stop is `area`'s `{` (Callers `area()`, `main()`; no message).
- **`-c N` (maxCalls):** when the stack has at least N frames, Callers shows the first N-1
  and then a `[...]` row (`-c 1`: just `[...]`, highlighted; `-c 2` with 2 frames: `area()`,
  `[...]`; `-c 3` with 2 frames: no `[...]`; with 3: `area()`, `main()`, `[...]`).
- **`-e N` (maxArrayElements):** `counts: [ +1 +2  ...]` (two spaces before `...`);
  vertically the last row is `...]` indented one column past the `[ 0]` column
  (`           ...]` for `counts: [ [ 0]: +1`).

## Resources
- **automaticBreakpoints: YES** sets a breakpoint on every function's `{` at startup.
- **scalarJustification:** scalars (ints, chars, floats; not pointers) take fixed-width fields
  after the single space: 11 for 4-byte integers, 6 for 2-byte, 6 for 1-byte (incl. chars:
  `tag:   '\0'`, `greeting: ->    'h'`), 12 for float. `right` pads left, `left` pads right,
  `center` puts floor(pad/2) left. Arrays: `counts: [          +1          +2 ... +5 ]`
  (right), `[     +1 ...      +5      ]` (center), `[ +1          +2 ...  +5          ]`
  (left).
- **maxString: 5:** `greeting: "hello"...`.
- **signedIntegerStyles / unsignedIntegerStyles / floatStyles** set per-size defaults
  (`hex - hex`: 1- and 4-byte signed in hex, `w: 0x20014a80`, `big: 0x075bcd15`;
  unsigned `signed`: `flags: -127`; float `scientific decimal`: double `scale: 1.5`).
  floatDigits 3 4 had no visible effect on these values.
- **menuInfoForeground** colours the non-selectable menu lines (`Detail:`, `Style:`);
  menuInfoBackground has no visible effect. **cursorForeground** is the input cursor's
  outline colour (default `#fa1340`); cursorBackground only becomes the GC background.

## commandList user commands
- `""=null` renders a blank row; clicking it (or any null row) shows the busy pointer and
  rings the bell. A name without padding runs into its key: `MainAlt-q`. `+` puts the 12
  defaults first; the two custom rows here followed **two** blank rows (`+` ends with one).
- Clicking `Area` (`area()`) opens a **compact prompt `Enter function parameter (in hex)`**
  (not in the catalogue) prefilled with the Function parameter address (`2ff22be8`, no `0x`),
  cursor after the text. Frame position (556,226) for a click at (761,288): centred on the
  pointer, clamped to the frame's right edge, its bottom at the pointer. Proceed: `Missing
  debugger glue function. Perhaps the code was linked without -g.` (our hand-assembled `rich`
  lacks xlC's glue); a successful call was not observable.

## Not observable on this guest
Fork/multiprocess (bsh segfaults under the emulator before forking), attach, normal exit
and xldb's exit status.

## Milestone 6 recordings (`m7scen/`, `m8scen/`)
- `scenario_m7.py` against `xldb -i segv -i 19 -r area -c 2 -e 2 ./rich` and
  `scenario_m8.py` against `xldb -co ./rich` (core from dbx, `cont SIGSEGV` in area).
- The terminated display also **clears the Subprograms highlight** (a Source click after
  choosing `area()` in Subprograms re-shows the terminated state without the cyan bar).
- In core mode the new stop sign is first drawn offset (the pass 12 quirk), then corrected.
