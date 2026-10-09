#!/usr/bin/env python3
"""Build-local CDFS fixes; never modify the caller's PS2SDK checkout/install."""
import argparse
from pathlib import Path
import shutil

NEXT_COMPONENT = r'''static char *nextPathComponent(char **cursor) {
    char *start = *cursor;
    while (*start == '/' || *start == '\\') start++;
    if (!*start) { *cursor = start; return NULL; }
    char *end = start;
    while (*end && *end != '/' && *end != '\\') end++;
    if (*end) *end++ = 0;
    *cursor = end;
    return start;
}

'''


def replace(text, old, new):
    if text.count(old) != 1:
        raise RuntimeError(f"Unsupported PS2SDK CDFS revision: {old!r}")
    return text.replace(old, new, 1)


def prepare(sdk, output):
    source = sdk / "iop/cdvd/cdfs/src"
    output.mkdir(parents=True, exist_ok=True)
    main = (source / "main.c").read_text()
    # Snapshot enumeration on EE keeps at most one physical directory open.
    # 512 records cover graphics/; fail packaging rather than silently truncate.
    main = replace(main, "#define MAX_FILES_PER_FOLDER 256", "#define MAX_FILES_PER_FOLDER 512")
    main = replace(main, "#define MAX_FOLDERS_OPENED 4", "#define MAX_FOLDERS_OPENED 1")
    main = replace(main, "return -EPERM;\n    }\n\n    entry = fod_table[i].entries[filesIndex];",
                   "return 0;\n    }\n\n    entry = fod_table[i].entries[filesIndex];")
    main = replace(main, "ret = cdfs_findfile(name, &entry);",
                   "ret = cdfs_findfile(name, &entry);\n    if (!ret) return -ENOENT;\n    memset(stat, 0, sizeof(*stat));")
    main = replace(main, "return ret;\n}\n\nIOMAN_RETURN_VALUE_IMPL", "return 0;\n}\n\nIOMAN_RETURN_VALUE_IMPL")
    main = replace(main, '#define DRIVER_MINOR_VERSION 2', '#define DRIVER_MINOR_VERSION 3')
    main = replace(main, 'printf("Re-edited by fjtrujy\\n");',
                   'printf("C-Dogs CDFS: 512 entries; EE snapshots; local path tokenizer\\n");\n    printf("Re-edited by fjtrujy\\n");')
    parser = (source / "cdfs_iop.c").read_text()
    parser = replace(parser, 'tocEntry->fileSize = internalTocEntry->fileSize;',
                     'memset(tocEntry, 0, sizeof(*tocEntry));\n    tocEntry->fileSize = internalTocEntry->fileSize;')
    parser = replace(parser, '     // setup the cdReadMode structure',
                     '    if (!cacheInfoDir.cache) return -1;\n\n     // setup the cdReadMode structure')
    main = replace(main, '    cdfs_prepare();',
                   '    if (cdfs_prepare() != 0) return MODULE_NO_RESIDENT_END;')
    # ROM sysclib strtok is global to IOP modules: a CD read yields to other
    # threads, which may use it. Paths must keep their own parsing cursor.
    parser = replace(parser, 'strtok(tocEntry->filename, ";");',
                     'char *version = strrchr(tocEntry->filename, \';\');\n        if (version) *version = 0;')
    parser = replace(parser, 'static int findPath(char *pathname) {',
                     NEXT_COMPONENT + 'static int findPath(char *pathname) {\n    char *pathCursor = pathname;')
    parser = replace(parser, 'dirname = strtok(pathname, "\\\\/");',
                     'dirname = nextPathComponent(&pathCursor);')
    parser = replace(parser, 'dirname = strtok(NULL, "\\\\/");',
                     'dirname = nextPathComponent(&pathCursor);')
    parser = replace(parser, '(localTocEntry.fileSize >> 11) + ((cdVolDesc.rootToc.tocSize & 2047) != 0)',
                     '(localTocEntry.fileSize >> 11) + ((localTocEntry.fileSize & 2047) != 0)')
    # Distinct module identity permits replacing the stock driver after SDL2main.
    header = replace((source / "cdfs_iop.h").read_text(),
                     '#define MODNAME "cdfs_driver"', '#define MODNAME "cdogs_cdfs"')
    for name, text in (("main.c", main), ("cdfs_iop.c", parser), ("cdfs_iop.h", header)):
        (output / name).write_text("/* Modified for C-Dogs SDL PS2: directory limits, EOF/stat, local path parsing.\n"
                                  " * Original PS2SDK sources/license and revision: platform/ps2/deps.lock.json.\n"
                                  " * Modifications: platform/ps2/tools/prepare_cdfs.py in the ps2-port branch. */\n" + text)
    for name in ("imports.lst", "irx_imports.h"):
        shutil.copy2(source / name, output / name)
    print(f"Prepared game-local CDFS in {output}")


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("sdk", type=Path)
    p.add_argument("output", type=Path)
    a = p.parse_args()
    prepare(a.sdk, a.output)
