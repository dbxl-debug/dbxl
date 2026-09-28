# dbxl's GDB helper (DESIGN.md 7.3): typed value trees over MI.
#
#   -dbxl-values locals LEVEL [--deref PATH]...
#   -dbxl-values globals [--deref PATH]...
#
# Every variable becomes a node:
#   n   name (children: field name or index)
#   k   kind: int uint char uchar bool float enum pointer array struct
#       union func other error
#   t   type name for menus, xldb style ("unsigned-char", "pointer", ...)
#   ts  the "type" style text ("int", "-> shape-struct", "[0..3] point-struct")
#   tag struct/union/enum tag ("shape"), for "shape{}"
#   sz  size in bytes, a the object's address (hex, "" if none)
#   v   value: integer (decimal), float (repr), enumerator name, pointer
#       (hex), or an error text for k=error
#   b   the value's bytes as a little-endian hex integer (hex styles)
#   s   pointers and char arrays: the C string there (at most 256 bytes)
#   e   a GDB expression for the object (for Edit)
#   lo  arrays: the low bound
#   ch  children (arrays, structs), p the pointee (only for --deref paths)
#
#   -dbxl-registers LEVEL NAME...    registers by GDB name; "xmm0:64" and
#                                    "xmm0:32" read an XMM register's low lane
#   -dbxl-symbols                    the program's files and functions
#   -dbxl-data-start                 where the program's data begins
#
# Paths name a node: the variable's name, then ".field", "[i]" or "*"
# (through a pointer), e.g. "sp*.corners*".
import gdb

MAX_ELEMENTS = 1000
MAX_STRING = 256


def _hyphen(name):
    return "-".join(name.split())


def _tag(t):
    t = t.strip_typedefs()
    return t.tag or t.name or ""


def _type_style(t):
    t = t.strip_typedefs()
    c = t.code
    if c == gdb.TYPE_CODE_PTR:
        return "-> " + _type_style(t.target())
    if c == gdb.TYPE_CODE_ARRAY:
        lo, hi = t.range()
        return "[%d..%d] %s" % (lo, hi, _type_style(t.target()))
    if c == gdb.TYPE_CODE_STRUCT:
        return _tag(t) + "-struct"
    if c == gdb.TYPE_CODE_UNION:
        return _tag(t) + "-union"
    if c == gdb.TYPE_CODE_ENUM:
        return _tag(t) + "-enum"
    if c == gdb.TYPE_CODE_FUNC:
        return "function"
    return _hyphen(str(t))


def _kind(t):
    c = t.code
    if c == gdb.TYPE_CODE_INT:
        name = str(t)
        if t.sizeof == 1 and "char" in name:
            return "uchar" if "unsigned" in name else "char"
        return "int" if t.is_signed else "uint"
    if c == gdb.TYPE_CODE_CHAR:
        return "uchar" if not t.is_signed else "char"
    if c == gdb.TYPE_CODE_BOOL:
        return "bool"
    if c == gdb.TYPE_CODE_FLT:
        return "float"
    if c == gdb.TYPE_CODE_ENUM:
        return "enum"
    if c == gdb.TYPE_CODE_PTR:
        return "pointer"
    if c == gdb.TYPE_CODE_ARRAY:
        return "array"
    if c == gdb.TYPE_CODE_STRUCT:
        return "struct"
    if c == gdb.TYPE_CODE_UNION:
        return "union"
    if c == gdb.TYPE_CODE_FUNC:
        return "func"
    return "other"


def _menu_title(kind, t):
    return {"pointer": "pointer", "array": "array", "struct": "structure",
            "union": "union", "enum": "enum", "func": "function"}.get(
                kind, _hyphen(str(t)))


def _bytes_hex(v):
    try:
        data = bytes(v.bytes)
    except (gdb.error, AttributeError):
        return ""
    return "%x" % int.from_bytes(data, "little")


def _c_string(addr):
    if addr == 0:
        return None
    inf = gdb.selected_inferior()
    out = bytearray()
    try:
        while len(out) < MAX_STRING:
            chunk = bytes(inf.read_memory(addr + len(out), 16))
            nul = chunk.find(b"\0")
            if nul >= 0:
                out += chunk[:nul]
                return out.decode("latin-1")
            out += chunk
    except gdb.MemoryError:
        if not out:
            return None
    return out.decode("latin-1")


def _node(v, name, expr, path, deref):
    node = {"n": name, "e": expr}
    try:
        t = v.type
        st = t.strip_typedefs()
        kind = _kind(st)
        node.update({"k": kind, "t": _menu_title(kind, st),
                     "ts": _type_style(t), "tag": _tag(st),
                     "sz": str(st.sizeof)})
        try:
            node["a"] = "%x" % int(v.address) if v.address is not None else ""
        except gdb.error:
            node["a"] = ""
        if v.is_optimized_out:
            node.update({"k": "error", "v": "<optimized out>"})
            return node
        if kind in ("int", "uint", "char", "uchar", "bool"):
            node["v"] = str(int(v))
            node["b"] = _bytes_hex(v)
        elif kind == "float":
            node["v"] = repr(float(v))
            node["b"] = _bytes_hex(v)
        elif kind == "enum":
            n = int(v)
            node["v"] = str(n)
            for f in st.fields():
                if f.enumval == n:
                    node["v"] = f.name
                    break
            node["b"] = _bytes_hex(v)
        elif kind == "pointer":
            addr = int(v)
            node["v"] = "%x" % addr
            node["b"] = node["v"]
            s = _c_string(addr)
            if s is not None:
                node["s"] = s
            if path + "*" in deref and addr != 0:
                node["p"] = _node(v.dereference(), "", "(*%s)" % expr,
                                  path + "*", deref)
        elif kind == "array":
            lo, hi = st.range()
            node["lo"] = str(lo)
            ch = []
            for i in range(lo, min(hi, lo + MAX_ELEMENTS - 1) + 1):
                ch.append(_node(v[i], str(i), "(%s)[%d]" % (expr, i),
                                "%s[%d]" % (path, i), deref))
            node["ch"] = ch
            if st.target().strip_typedefs().sizeof == 1 and v.address is not None:
                s = _c_string(int(v.address))
                if s is not None:
                    node["s"] = s[:hi - lo + 1]
        elif kind in ("struct", "union"):
            ch = []
            for f in st.fields():
                if f.is_base_class or not f.name:
                    continue
                ch.append(_node(v[f.name], f.name, "(%s).%s" % (expr, f.name),
                                "%s.%s" % (path, f.name), deref))
            node["ch"] = ch
        else:
            node["v"] = str(v)
    except gdb.MemoryError:
        node.update({"k": "error", "v": "<unreadable>"})
    except gdb.error as e:
        node.update({"k": "error", "v": "<%s>" % e})
    return node


def _frame(level):
    f = gdb.newest_frame()
    for _ in range(level):
        f = f.older()
        if f is None:
            raise gdb.GdbError("no frame %d" % level)
    return f


def _locals(level, deref):
    frame = _frame(level)
    try:
        block = frame.block()
    except RuntimeError:
        return []
    # From the function's block down to the innermost one (outer first).
    blocks = []
    while block is not None:
        blocks.append(block)
        if block.function is not None:
            break
        block = block.superblock
    blocks.reverse()
    args, local_vars, seen = [], [], set()
    for b in blocks:
        for sym in b:
            if not (sym.is_argument or sym.is_variable) or sym.name in seen:
                continue
            seen.add(sym.name)
            (args if sym.is_argument else local_vars).append(sym)
    out = []
    for sym in args + local_vars:
        try:
            v = sym.value(frame)
        except gdb.error as e:
            out.append({"n": sym.name, "k": "error", "v": "<%s>" % e})
            continue
        out.append(_node(v, sym.name, sym.name, sym.name, deref))
    return out


def _lookup(name, fullname):
    """The global or file-static symbol `name` defined in `fullname`."""
    cands = []
    sym = gdb.lookup_global_symbol(name)
    if sym is not None:
        cands.append(sym)
    try:
        cands += gdb.lookup_static_symbols(name)
    except AttributeError:
        sym = gdb.lookup_static_symbol(name)
        if sym is not None:
            cands.append(sym)
    for sym in cands:
        if sym.symtab is not None and sym.symtab.fullname() == fullname:
            return sym
    return None


def _globals(deref):
    try:
        info = gdb.execute_mi("-symbol-info-variables")
    except gdb.error:
        return []
    progname = gdb.current_progspace().filename
    out = []
    for f in info.get("symbols", {}).get("debug", []):
        fullname = f.get("fullname") or f.get("filename")
        entries = sorted(f.get("symbols", []), key=lambda e: int(e.get("line", 0)))
        nodes = []
        for e in entries:
            sym = _lookup(e["name"], fullname)
            if sym is None or sym.symtab.objfile.filename != progname:
                continue
            try:
                v = sym.value()
            except gdb.error as err:
                nodes.append({"n": sym.name, "k": "error", "v": "<%s>" % err})
                continue
            nodes.append(_node(v, sym.name, sym.name, sym.name, deref))
        if nodes:
            out.append({"file": f.get("filename", "").split("/")[-1],
                        "fullname": fullname, "vars": nodes})
    return out


def _registers(level, names):
    frame = _frame(level)
    out = []
    for n in names:
        base, _, lane = n.partition(":")
        try:
            v = frame.read_register(base)
            if lane:
                bits = int(lane)
                raw = int(v["uint128"])
                val, size = raw & ((1 << bits) - 1), bits // 8
            else:
                # Raw bytes: flag types (eflags, mxcsr) don't cast to int.
                data = bytes(v.bytes)
                size = len(data)
                val = int.from_bytes(data, "little")
            out.append({"n": n, "v": "%x" % val, "sz": str(size)})
        except (gdb.error, ValueError, KeyError) as e:
            out.append({"n": n, "v": "", "sz": "0", "err": str(e)})
    return out


def _symbols():
    """Files and functions with debug info in the program itself."""
    progname = gdb.current_progspace().filename
    try:
        info = gdb.execute_mi("-symbol-info-functions")
    except gdb.error:
        return [], []
    files, funcs, seen = [], [], set()
    for f in info.get("symbols", {}).get("debug", []):
        fullname = f.get("fullname") or f.get("filename")
        for e in f.get("symbols", []):
            sym = gdb.lookup_global_symbol(e["name"]) or gdb.lookup_static_symbol(e["name"])
            if sym is None or sym.symtab is None or sym.symtab.objfile.filename != progname:
                continue
            try:
                start = sym.value().address
                addr = int(start)
                line = gdb.find_pc_line(addr).line or int(e.get("line", 0))
            except gdb.error:
                addr, line = 0, int(e.get("line", 0))
            funcs.append({"name": e["name"], "file": f.get("filename", ""),
                          "fullname": fullname, "line": str(line),
                          "addr": "%x" % addr})
            if fullname not in seen:
                seen.add(fullname)
                files.append({"file": f.get("filename", "").split("/")[-1],
                              "fullname": fullname})
    files.sort(key=lambda x: x["file"])
    funcs.sort(key=lambda x: x["name"])
    return files, funcs


def _data_start():
    for name in ("__data_start", "data_start", "__DTOR_LIST__"):
        try:
            return int(gdb.parse_and_eval("&" + name).cast(
                gdb.lookup_type("unsigned long long")))
        except gdb.error:
            continue
    return 0


class DbxlRegisters(gdb.MICommand):
    def __init__(self):
        super().__init__("-dbxl-registers")

    def invoke(self, argv):
        return {"regs": _registers(int(argv[0]) if argv else 0, argv[1:])}


class DbxlSymbols(gdb.MICommand):
    def __init__(self):
        super().__init__("-dbxl-symbols")

    def invoke(self, argv):
        files, funcs = _symbols()
        return {"files": files, "functions": funcs}


class DbxlDataStart(gdb.MICommand):
    def __init__(self):
        super().__init__("-dbxl-data-start")

    def invoke(self, argv):
        return {"addr": "%x" % _data_start()}


class DbxlValues(gdb.MICommand):
    def __init__(self):
        super().__init__("-dbxl-values")

    def invoke(self, argv):
        if not argv:
            raise gdb.GdbError("-dbxl-values: locals LEVEL | globals")
        deref, rest, i = set(), [], 1
        while i < len(argv):
            if argv[i] == "--deref" and i + 1 < len(argv):
                deref.add(argv[i + 1])
                i += 2
            else:
                rest.append(argv[i])
                i += 1
        ptrsize = str(gdb.lookup_type("void").pointer().sizeof)
        if argv[0] == "locals":
            level = int(rest[0]) if rest else 0
            return {"ptrsize": ptrsize, "vars": _locals(level, deref)}
        if argv[0] == "globals":
            return {"ptrsize": ptrsize, "files": _globals(deref)}
        raise gdb.GdbError("-dbxl-values: unknown scope " + argv[0])


DbxlValues()
DbxlRegisters()
DbxlSymbols()
DbxlDataStart()
