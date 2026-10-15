#!/usr/bin/env python3
"""Pass 2 and 3 of the rename: computes snake_case names for what collect.py found and rewrites every
recorded spelling, plus the identifiers inside macro bodies.

    tools/rename/rename.py collect.json [--apply] [--overrides tools/rename/overrides.toml]

Without --apply it only writes rename-map.tsv (old -> new per declaration) and reports collisions.
"""
import argparse, collections, json, re, sys, tomllib
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]

KEYWORDS = set('''alignas alignof and and_eq asm auto bitand bitor bool break case catch char char8_t char16_t
char32_t class compl concept const consteval constexpr constinit const_cast continue co_await co_return
co_yield decltype default delete do double dynamic_cast else enum explicit export extern false float for
friend goto if inline int long mutable namespace new noexcept not not_eq nullptr operator or or_eq private
protected public register reinterpret_cast requires return short signed sizeof static static_assert
static_cast struct switch template this thread_local throw true try typedef typeid typename union
unsigned using virtual void volatile wchar_t while xor xor_eq'''.split())
# C library names a free function or global must not shadow inside metin2.
C_NAMES = set('''abs exp log time random rand srand min max index select read write open close connect send recv
accept socket bind listen kill signal sleep wait remove rename link unlink stat access floor ceil round
pow sqrt sin cos tan atan atan2 div labs strlen strcpy memset memcpy printf sprintf fopen fclose exit
abort atoi atol strtol getenv system clock difftime mktime localtime gmtime strftime isdigit isspace
tolower toupper qsort bsearch errno assert major minor'''.split())

HUNGARIAN = re.compile(r'^(?:c_p|c_|dw|w|by|b|l|ll|i|n|f|d|sz|st|str|pk|lp|p|ui|ul|u|v|vec|map|set|lst|list|it|e|a|ab|ai|al|adw|aw|k|rk|r|h|q)(?=[A-Z])')
TYPE_PREFIX = re.compile(r'^[CTSEI](?=[A-Z]+[a-z])')


def words(name):
    name = name.strip('_')
    parts = []
    for chunk in re.split(r'_+', name):
        parts += re.findall(r'[A-Z]+(?=[A-Z][a-z0-9]|$|_)|[A-Z]?[a-z0-9]+|[A-Z]+|\d+', chunk)
    return [p.lower() for p in parts if p]


def snake(name):
    return '_'.join(words(name)) or name.lower()


def strip_hungarian(name):
    for prefix in ('m_', 'g_', 's_', 'gs_', 'ms_', 'c_'):
        if name.startswith(prefix):
            name = name[len(prefix):]
            break
    return HUNGARIAN.sub('', name) or name


def safe(name):
    return name + '_' if name in KEYWORDS else name


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('collected')
    ap.add_argument('--apply', action='store_true')
    ap.add_argument('--overrides', default=str(REPO / 'tools/rename/overrides.toml'))
    a = ap.parse_args()

    data = json.load(open(a.collected))
    decls = {u: d for u, d in data['decls'].items() if re.fullmatch(r'[A-Za-z_]\w*', d['name'])}
    overrides = tomllib.load(open(a.overrides, 'rb')) if Path(a.overrides).exists() else {}
    # [types], [functions], [globals], [methods], [fields]: old name -> new name for that kind
    def forced(kind, name):
        return overrides.get(kind + 's', {}).get(name)
    keep_get = set(overrides.get('keep_get', []))  # getters that stay get_x
    ALIAS = {'TYPEDEF_DECL', 'TYPE_ALIAS_DECL', 'TYPE_ALIAS_TEMPLATE_DECL'}

    def role(scope):
        ns = scope[1] if scope[0] == 'namespace' else ''
        return 'db' if '::db' in ns else ('game' if '::game' in ns else 'shared')

    # types first: their new names decide getter collisions
    new = {}
    for usr, d in decls.items():
        if d['kind'] == 'type':
            n = forced('type', d['name']) or safe(snake(TYPE_PREFIX.sub('', d['name'])))
            new[usr] = n
    type_names = set(new.values())
    free_names = set()
    for usr, d in decls.items():
        if d['kind'] in ('function', 'global'):
            if d['kind'] == 'global' and re.fullmatch(r'[A-Z][A-Z0-9_]*', d['name']):
                new[usr] = d['name']   # constants keep their UPPER_CASE names
                continue
            base = strip_hungarian(d['name']) if d['kind'] == 'global' else d['name']
            n = forced(d['kind'], d['name']) or safe(snake(base))
            if n in C_NAMES or (d['kind'] == 'global' and n in type_names and not forced('global', d['name'])):
                n = safe(snake(d['name']))
            new[usr] = n
            free_names.add(n)

    # member functions: one decision per old name, so overrides stay consistent
    method_names = collections.defaultdict(list)
    for usr, d in decls.items():
        if d['kind'] == 'method':
            method_names[d['name']].append(usr)
    fields_by_class = collections.defaultdict(set)
    for usr, d in decls.items():
        if d['kind'] == 'field':
            fields_by_class[d['scope'][1]].add(snake(strip_hungarian(d['name'])))
    # inline namespaces collapse into their parent for name lookup
    import subprocess
    inline = set(re.findall(r'namespace (metin2(?:::\w+)*)::inline (\w+)', subprocess.run(
        ['grep', '-rhoE', r'namespace metin2(::[a-z_]+)*::inline [a-z_]+', 'src', 'tests'], cwd=REPO,
        capture_output=True, text=True).stdout))

    def effective(ns):
        parts = []
        for part in ns.split('::') if ns else []:
            if part and ('::'.join(parts), part) not in inline:
                parts.append(part)
        return parts

    types_in = collections.defaultdict(list)   # new type name -> effective namespaces declaring it
    for usr, d in decls.items():
        if d['kind'] == 'type' and d['scope'][0] == 'namespace':
            types_in[new[usr]].append(effective(d['scope'][1]))

    def class_namespace(cls):
        d = decls.get(cls)
        while d is not None and d['scope'][0] == 'class':
            d = decls.get(d['scope'][1])
        return effective(d['scope'][1]) if d else []

    def type_visible(name, cls):
        here = class_namespace(cls)
        return any(here[:len(ns)] == ns for ns in types_in.get(name, []))

    # names a member must not take within its class: the final names of the other members
    members = collections.defaultdict(set)
    for usr, d in decls.items():
        if d['kind'] == 'method':
            members[d['scope'][1]].add(safe(snake(d['name'])))
        elif d['kind'] == 'field':
            base = forced('field', d['name']) or snake(strip_hungarian(d['name']))
            members[d['scope'][1]].add(base + ('_' if d['access'] in ('PRIVATE', 'PROTECTED') else ''))
    # bases of each class, transitively: a getter must not clash in the classes it inherits from
    def ancestry(cls, seen=None):
        seen = set() if seen is None else seen
        for b in decls.get(cls, {}).get('bases', []):
            if b not in seen:
                seen.add(b); ancestry(b, seen)
        return seen

    def method_name(old, classes):
        if forced('method', old):
            return forced('method', old)
        w = words(old)
        if len(w) > 1 and w[0] == 'get' and old not in keep_get:
            short = safe('_'.join(w[1:]))
            scopes = set(classes)
            for c in classes:
                scopes |= ancestry(c)
            if short in C_NAMES or any(type_visible(short, c) for c in classes) or any(short in members[c] for c in scopes):
                return safe('_'.join(w))
            return short
        return safe(snake(old))

    # virtual methods: one name per old name across all classes, so overrides keep matching
    virtual_names = {d['name'] for d in decls.values() if d['kind'] == 'method' and (d['virtual'] or d['overrides'])}
    for old, usrs in method_names.items():
        if old in virtual_names:
            n = method_name(old, [decls[u]['scope'][1] for u in usrs])
            for u in usrs:
                new[u] = n
        else:
            for u in usrs:
                new[u] = method_name(old, [decls[u]['scope'][1]])

    # data members: private/protected get a trailing underscore; collisions within a class fall back
    by_class = collections.defaultdict(list)
    for usr, d in decls.items():
        if d['kind'] == 'field':
            by_class[d['scope'][1]].append(usr)
    for cls, usrs in by_class.items():
        seen = {}
        for usr in sorted(usrs, key=lambda u: decls[u]['name']):
            d = decls[usr]
            base = forced('field', d['name']) or snake(strip_hungarian(d['name']))
            if base in seen:
                base = snake(d['name'])
            seen[base] = usr
            suffix = '_' if d['access'] in ('PRIVATE', 'PROTECTED') else ''
            new[usr] = safe(base) + suffix if not suffix else base + suffix

    # clashes: different declarations that end up with one name in one scope
    def scope_key(d):
        if d['scope'][0] == 'class':
            return ('class', d['scope'][1])
        return ('namespace', '::'.join(effective(d['scope'][1])))

    def clashes():
        groups = collections.defaultdict(dict)   # scope -> new name -> {(kind, old name)}
        aliases = collections.defaultdict(set)
        for usr, d in decls.items():
            if d['kind'] in ('method', 'field', 'type', 'function', 'global'):
                groups[scope_key(d)].setdefault(new[usr], set()).add((d['kind'], d['name']))
                if d.get('ckind') in ALIAS:
                    aliases[(scope_key(d), new[usr])].add(d['name'])
        found = []
        for scope, names in groups.items():
            for name, olds in names.items():
                if len(olds) < 2:
                    continue
                if all(k == 'type' for k, _ in olds) and aliases[(scope, name)]:
                    continue
                found.append((scope, name, olds))
        # a function, global or type hiding a different one of an enclosing namespace
        for scope, names in groups.items():
            if scope[0] != 'namespace' or not scope[1]:
                continue
            parts = scope[1].split('::')
            for depth in range(len(parts)):
                outer = ('namespace', '::'.join(parts[:depth]))
                for name, olds in names.items():
                    theirs = groups.get(outer, {}).get(name)
                    if not theirs or theirs == olds:
                        continue
                    alias_pair = all(k == 'type' for k, _ in olds | theirs) and (aliases[(scope, name)] or aliases[(outer, name)])
                    if not alias_pair:
                        found.append((scope, name, olds | {(k, f'{o} (in {outer[1] or "::"})') for k, o in theirs}))
        return found

    for _ in range(5):
        changed = False
        for scope, name, olds in clashes():
            for kind, old in olds:
                w = words(old)
                if kind == 'method' and len(w) > 1 and w[0] == 'get' and name != safe('_'.join(w)):
                    for usr, d in decls.items():
                        if d['kind'] == 'method' and d['name'] == old and (scope[0] != 'class' or d['scope'][1] == scope[1] or old in virtual_names):
                            new[usr] = safe('_'.join(w)); changed = True
        if not changed:
            break

    # captures: a renamed reference inside a function that declares a local or parameter with the new name
    # would bind to that local instead
    local_decls = data.get('locals', {})   # function -> local name -> local usrs
    local_names = {k: set(v) for k, v in local_decls.items()}
    texts = {}
    def qualified(file, line, col):
        """Whether the name at file:line:col follows `.`, `->` or `::` (those cannot bind to a local)."""
        if file not in texts:
            texts[file] = open(file, 'rb').read().split(b'\n')
        lines = texts[file]
        l, i = line - 1, col - 2
        while l >= 0:
            while i >= 0 and lines[l][i] in b' \t':
                i -= 1
            if i >= 0:
                return lines[l][i:i + 1] == b'.' or lines[l][i - 1:i + 1] in (b'->', b'::') if i >= 1 else lines[l][i:i + 1] == b'.'
            l -= 1
            i = len(lines[l]) - 1 if l >= 0 else -1
        return False

    def captures():
        found = collections.defaultdict(set)
        for file, line, col, usr, name, fn, cls in data["spots"]:
            if fn and usr in decls and usr in new and new[usr] != name and decls[usr]['kind'] in ('field', 'global', 'function', 'method'):
                if new[usr] in local_names.get(fn, ()) and not qualified(file, line, col):
                    found[usr].add((fn, f'{Path(file).relative_to(REPO)}:{line}'))
        return found

    # types hidden by a local declared earlier in the same function: qualify the reference
    local_usrs = {u for names in local_decls.values() for us in names.values() for u in us}
    local_at = {}
    for file, line, col, usr, name, fn, cls in data["spots"]:
        if usr in local_usrs:
            local_at[usr] = min(local_at.get(usr, (file, line, col)), (file, line, col))
    def qualified_name(d, file):
        ns = '::'.join(p for p in d['scope'][1].split('::') if p)
        inside = file.startswith(str(REPO / 'src/metin2') + '/')
        if inside and ns.startswith('metin2::'):
            ns = ns[len('metin2::'):]
        elif inside and ns == 'metin2':
            ns = ''
        return (ns + '::' if ns else '::metin2::' if not inside else 'metin2::') + new[d['usr']]

    # final member names per class, with those of its bases
    member_names = collections.defaultdict(set)
    for usr, d in decls.items():
        if d['scope'][0] == 'class' and d['kind'] in ('field', 'method', 'type'):
            member_names[d['scope'][1]].add(new[usr])

    def visible_members(cls):
        names = set(member_names.get(cls, ()))
        for base in ancestry(cls):
            names |= member_names.get(base, set())
        return names

    qualify = {}
    for file, line, col, usr, name, fn, cls in data["spots"]:
        d = decls.get(usr)
        if d is None or d['scope'][0] != 'namespace' or d['kind'] not in ('type', 'function', 'global'):
            continue
        if new[usr] == name and not cls:
            continue
        if qualified(file, line, col):
            continue
        d = dict(d, usr=usr)
        # hidden by a member of the enclosing class (or its bases)
        if cls and new[usr] in visible_members(cls):
            qualify[(file, line, col)] = qualified_name(d, file)
            continue
        # a type hidden by a local declared earlier in the same function
        if fn and d['kind'] == 'type':
            for local in local_decls.get(fn, {}).get(new[usr], ()):
                at = local_at.get(local)
                if at and at[0] == file and at[1:] < (line, col):
                    qualify[(file, line, col)] = qualified_name(d, file)
                    break

    # rename the capturing locals: `int x = GetX();` becomes `int x_value = x();`
    renamed_locals = []
    for usr, where in captures().items():
        for fn, at in where:
            name = new[usr]
            if name not in local_names.get(fn, ()):
                continue
            replacement = name + '_value'
            while replacement in local_names[fn]:
                replacement += '_'
            for local in local_decls[fn][name]:
                new[local] = replacement
            local_names[fn].discard(name)
            local_names[fn].add(replacement)
            renamed_locals.append(f'{at}: local {name} -> {replacement}')

    # report
    out = REPO / 'tools/rename/rename-map.tsv'
    with open(out, 'w') as f:
        for usr, d in sorted(decls.items(), key=lambda x: (x[1]['kind'], x[1]['name'])):
            if new.get(usr) and new[usr] != d['name']:
                f.write(f"{d['kind']}\t{d['scope'][1]}\t{d['name']}\t{new[usr]}\n")
    print(f"{sum(1 for u in new if u in decls and new[u] != decls[u]['name'])} renames written to {out}")
    for scope, name, olds in sorted(clashes(), key=lambda x: (x[0], x[1])):
        where = decls[scope[1]]['name'] if scope[0] == 'class' and scope[1] in decls else scope[1]
        print(f'collision in {where or "::"}: {name} <- {sorted(olds)}')

    print(f'{len(qualify)} type references qualified because a local hides the type')
    for line in sorted(set(renamed_locals)):
        print(line)
    caught = captures()
    for usr, where in sorted(caught.items(), key=lambda x: decls[x[0]]['name']):
        d = decls[usr]
        print(f"capture: {d['kind']} {d['name']} -> {new[usr]} at {', '.join(sorted(w for _, w in where)[:4])}")
    print(f'{len(renamed_locals)} locals renamed, {len(caught)} captured names left')

    if not a.apply:
        return

    # rewrite every recorded spelling
    spots = collections.defaultdict(dict)
    conflicts = set()
    for file, line, col, usr, name, fn, cls in data["spots"]:
        if usr in new and (new[usr] != name or (file, line, col) in qualify):
            seen = spots[file].get((line, col))
            if seen and seen[1].split('::')[-1] != new[usr]:
                conflicts.add((file, line, col, name, seen[1], new[usr]))
            spots[file][(line, col)] = (name, qualify.get((file, line, col), new[usr]))
    for c in sorted(conflicts):
        print('ambiguous spelling (macro?):', *c)
    for file, edits in spots.items():
        lines = open(file, 'rb').read().split(b'\n')
        for (line, col), (old, nw) in sorted(edits.items(), reverse=True):
            text, i = lines[line - 1], col - 1
            o, n = old.encode(), nw.encode()
            end = i + len(o)
            if text[i:end] == o and (end == len(text) or not (chr(text[end]).isalnum() or text[end] == 0x5f)):
                lines[line - 1] = text[:i] + n + text[end:]
            else:
                print(f'skipped {file}:{line}:{col}: expected {old}')
        open(file, 'wb').write(b'\n'.join(lines))

    # aliases that now name their own target: `typedef x x;`, `using x = x;`, `typedef struct x x;`
    forward_alias = re.compile(r'^([ \t]*)typedef\s+(struct|union|class)\s+(\w+)\s+\3\s*;', re.M)
    self_alias = re.compile(r'^[ \t]*(?:typedef\s+(\w+)\s+\1\s*;|using\s+(\w+)\s*=\s*\2\s*;)[ \t]*\n', re.M)
    for path in list((REPO / 'src').rglob('*')) + list((REPO / 'tests').rglob('*')):
        if path.suffix in ('.h', '.cpp', '.hpp'):
            s = path.read_text(encoding='utf-8', errors='surrogateescape')
            s2 = self_alias.sub('', forward_alias.sub(r'\1\2 \3;', s))
            if s2 != s:
                path.write_text(s2, encoding='utf-8', errors='surrogateescape')
    print('applied')


if __name__ == '__main__':
    main()
