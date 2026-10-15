#!/usr/bin/env python3
"""Pass 1 of the rename: records every declaration in src/ and tests/ with its kind, scope and name,
and every place its name is spelled (declarations and references), using libclang over the
compile_commands.json of a build directory.

    tools/rename/collect.py build out.json
"""
import json, os, shlex, subprocess, sys
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path

import ctypes
import clang.cindex as ci

K = ci.CursorKind
REPO = Path(__file__).resolve().parents[2]
ROOTS = (str(REPO / 'src') + '/', str(REPO / 'tests') + '/')
RESOURCE = subprocess.run(['clang', '-print-resource-dir'], capture_output=True, text=True).stdout.strip()

TYPE_KINDS = {K.CLASS_DECL, K.STRUCT_DECL, K.UNION_DECL, K.ENUM_DECL, K.CLASS_TEMPLATE, K.TYPEDEF_DECL,
              K.TYPE_ALIAS_DECL, K.TYPE_ALIAS_TEMPLATE_DECL}
FUNC_KINDS = {K.FUNCTION_DECL, K.CXX_METHOD, K.FUNCTION_TEMPLATE}
REF_KINDS = {K.TYPE_REF, K.DECL_REF_EXPR, K.MEMBER_REF_EXPR, K.MEMBER_REF, K.TEMPLATE_REF, K.OVERLOADED_DECL_REF}


_spelling = ci.conf.lib.clang_getSpellingLocation
_spelling.argtypes = [ci.SourceLocation, ctypes.POINTER(ci.c_object_p), ctypes.POINTER(ctypes.c_uint),
                      ctypes.POINTER(ctypes.c_uint), ctypes.POINTER(ctypes.c_uint)]
_file_name = ci.conf.lib.clang_getFileName


def spelled_at(cursor):
    """(file, line, column) where the cursor's name is written, also inside macro arguments and bodies."""
    f, line, col, off = ci.c_object_p(), ctypes.c_uint(), ctypes.c_uint(), ctypes.c_uint()
    _spelling(cursor.location, ctypes.byref(f), ctypes.byref(line), ctypes.byref(col), ctypes.byref(off))
    if not f:
        return None
    name = _file_name(ci.File(f))
    name = name if isinstance(name, str) else ci._CXString.from_result(name)
    return name, line.value, col.value


def ours(location):
    return location.file is not None and location.file.name.startswith(ROOTS)


def primary(cursor):
    """The template a specialization was instantiated from, or the cursor itself (also for the fields
    of a class template specialization, which libclang does not map back)."""
    while True:
        t = ci.conf.lib.clang_getSpecializedCursorTemplate(cursor)
        if t is None or t.is_null() or t == cursor:
            break
        t._tu = cursor._tu
        cursor = t
    parent = cursor.semantic_parent
    if cursor.kind == K.FIELD_DECL and parent is not None:
        template = primary(parent) if parent.kind != K.CLASS_TEMPLATE else parent
        if template != parent:
            for member in template.get_children():
                if member.kind == K.FIELD_DECL and member.spelling == cursor.spelling:
                    return member
    return cursor


def compile_args(entry):
    args = shlex.split(entry['command'])[1:]
    out, skip = [], False
    for a in args:
        if skip:
            skip = False
            continue
        if a in ('-o', '-c', '-MF', '-MT', '-MQ'):
            skip = True
            continue
        if a == entry['file'] or a.startswith('-W') or a in ('-MD', '-MMD'):
            continue
        out.append(a)
    return out + ['-resource-dir', RESOURCE, '-Wno-everything']


def scope_of(cursor):
    """('namespace', 'metin2::game') or ('class', usr) for the semantic parent."""
    parent = cursor.semantic_parent
    if parent is None or parent.kind == K.TRANSLATION_UNIT:
        return ['namespace', '']
    if parent.kind == K.NAMESPACE:
        names = []
        p = parent
        while p is not None and p.kind == K.NAMESPACE:
            names.append(p.spelling)
            p = p.semantic_parent
        return ['namespace', '::'.join(reversed(names))]
    if parent.kind in TYPE_KINDS | {K.CLASS_TEMPLATE_PARTIAL_SPECIALIZATION}:
        return ['class', parent.get_usr()]
    return ['local', parent.get_usr()]


def classify(cursor):
    k = cursor.kind
    if k in TYPE_KINDS:
        return 'type'
    if k in FUNC_KINDS:
        name = cursor.spelling
        if name.startswith('operator') or name == 'main':
            return None
        return 'method' if cursor.semantic_parent is not None and cursor.semantic_parent.kind in TYPE_KINDS else 'function'
    if k == K.FIELD_DECL:
        return 'field'
    if k == K.VAR_DECL:
        scope = scope_of(cursor)[0]
        if scope == 'namespace':
            return 'global'
        if scope == 'class':
            return 'field'
    return None


def collect(entry):
    index = ci.Index.create()
    try:
        tu = index.parse(entry['file'], args=compile_args(entry))
    except ci.TranslationUnitLoadError:
        return entry['file'], {}, [], ['parse failed'], {}
    errors = [str(d) for d in tu.diagnostics if d.severity >= 3]
    decls, spots, locals_, local_decls = {}, [], {}, {}
    BODIES = {K.FUNCTION_DECL, K.CXX_METHOD, K.CONSTRUCTOR, K.DESTRUCTOR, K.FUNCTION_TEMPLATE, K.CONVERSION_FUNCTION}

    def note(cursor, usr, name, fn='', cls=''):
        at = spelled_at(cursor)
        if at and at[0].startswith(ROOTS):
            spots.append([at[0], at[1], at[2], usr, name, fn, cls])

    def visit(cursor, fn='', cls=''):
        for child in cursor.get_children():
            child_fn, child_cls = fn, cls
            if child.kind in (K.CLASS_DECL, K.STRUCT_DECL, K.UNION_DECL, K.CLASS_TEMPLATE) and child.is_definition():
                child_cls = primary(child).get_usr()
            elif child.kind in (K.CXX_METHOD, K.CONSTRUCTOR, K.DESTRUCTOR, K.FUNCTION_TEMPLATE, K.CONVERSION_FUNCTION) \
                    and child.semantic_parent is not None and child.semantic_parent.kind in TYPE_KINDS:
                child_cls = primary(child.semantic_parent).get_usr()
            if child.kind in BODIES and child.is_definition() and ours(child.location):
                child_fn = f'{child.location.file.name}:{child.extent.start.line}:{child.get_usr()}'
            if fn and child.kind in (K.PARM_DECL, K.VAR_DECL) and child.spelling and scope_of(child)[0] == 'local':
                locals_.setdefault(fn, set()).add(child.spelling)
                note(child, child.get_usr(), child.spelling, fn, cls)
                local_decls.setdefault(fn, {}).setdefault(child.spelling, set()).add(child.get_usr())

            if child.location.file is not None and not ours(child.location) and child.kind != K.NAMESPACE:
                continue
            kind = classify(child)
            if kind and child.spelling:
                usr = child.get_usr()
                if usr and ours(child.location):
                    info = decls.setdefault(usr, {'kind': kind, 'ckind': child.kind.name, 'name': child.spelling, 'scope': scope_of(child),
                                                  'access': child.access_specifier.name, 'virtual': False, 'overrides': []})
                    if child.kind == K.CXX_METHOD:
                        info['virtual'] = info['virtual'] or child.is_virtual_method()
                        for base in child.get_overridden_cursors() if hasattr(child, 'get_overridden_cursors') else []:
                            info['overrides'].append(base.get_usr())
                    if child.kind == K.CLASS_DECL or child.kind == K.STRUCT_DECL:
                        info['bases'] = [b.type.get_canonical().get_declaration().get_usr() for b in child.get_children() if b.kind == K.CXX_BASE_SPECIFIER]
                    note(child, usr, child.spelling, fn, cls)
            if child.kind in (K.CONSTRUCTOR, K.DESTRUCTOR) and child.semantic_parent is not None:
                owner = child.semantic_parent
                at = spelled_at(child)
                if at and at[0].startswith(ROOTS):
                    col = at[2] + (1 if child.kind == K.DESTRUCTOR else 0)
                    spots.append([at[0], at[1], col, primary(owner).get_usr(), owner.spelling, '', ''])
            if child.kind == K.OVERLOADED_DECL_REF:
                for i in range(ci.conf.lib.clang_getNumOverloadedDecls(child)):
                    ref = ci.conf.lib.clang_getOverloadedDecl(child, i)
                    if ref is None or ref.is_null():
                        continue
                    ref._tu = child._tu
                    ref = primary(ref)
                    if ref.get_usr():
                        note(child, ref.get_usr(), ref.spelling, fn, cls)
            elif child.kind in REF_KINDS:
                ref = child.referenced
                if ref is not None:
                    ref = primary(ref)
                    if ref.get_usr():
                        note(child, ref.get_usr(), ref.spelling, fn, cls)
            visit(child, child_fn, child_cls)

    visit(tu.cursor)
    return entry['file'], decls, spots, errors, {k: {n: sorted(u) for n, u in v.items()} for k, v in local_decls.items()}


def main():
    build, out = sys.argv[1], sys.argv[2]
    entries = [e for e in json.load(open(Path(build) / 'compile_commands.json')) if e['file'].startswith(ROOTS)]
    all_decls, all_spots, problems, all_locals = {}, set(), {}, {}
    with ProcessPoolExecutor(max_workers=os.cpu_count()) as pool:
        for i, (file, decls, spots, errors, locals_) in enumerate(pool.map(collect, entries, chunksize=1), 1):
            for k, v in locals_.items():
                for n, u in v.items():
                    all_locals.setdefault(k, {}).setdefault(n, set()).update(u)
            for usr, info in decls.items():
                known = all_decls.setdefault(usr, info)
                known['virtual'] = known['virtual'] or info['virtual']
                known['overrides'] = sorted(set(known['overrides']) | set(info['overrides']))
                if 'bases' in info:
                    known['bases'] = info['bases']
            all_spots.update(tuple(s) for s in spots)
            if errors:
                problems[file] = errors[:3]
    json.dump({'decls': all_decls, 'spots': sorted(all_spots), 'locals': {k: {n: sorted(u) for n, u in v.items()} for k, v in all_locals.items()},
               'problems': problems}, open(out, 'w'))
    print(f'{len(all_decls)} declarations, {len(all_spots)} name spots, {len(problems)} files with parse errors')


if __name__ == '__main__':
    main()
