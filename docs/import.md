# Importing from the player's BTECH.EXE

Some of what the game shows is not in a data file but inside the program: the font and much of
the text. The rebuild never ships any of it (docs/rules.md, rule 1). Instead, an importer reads
the player's own BTECH.EXE once and writes that data as plain files in the player's data folder.
After that the game reads only those files; BTECH.EXE is needed only to import.

## When it runs

- By itself, the first time `chr` starts, and again whenever the data folder's import is missing
  or comes from another import version (`kImportVersion` in `src/formats/import.h`).
- On demand with `chr --import`, which imports again and quits.

If an import is needed and BTECH.EXE is missing or is not DOS 1.03, `chr` stops with a message
that says so and how to run the importer.

The data folder is `--data-dir DIR`, else the `CHR_DATA_DIR` environment variable, else the
user's application-data folder (on macOS `~/Library/Application Support/crescent-hawks-rebuild`).

## What it writes

    import.json          {"import_version": 1, "source": "BTECH.EXE DOS 1.03", "exe_sha256": ...}
    text/english.json    {"texts": [{"id": ..., "address": "ssss:oooo", "text": ...}, ...]}
    font/font.txt        the font (docs/formats/font.md)

`import.json` is written last, so an interrupted import counts as missing.

## How it reads the program

`src/formats/exepack.cpp` unpacks BTECH.EXE in memory, with the same steps as
`tools/unexepack.py`. The result's SHA-256 must be the DOS 1.03 program's (056b4067...).
`src/formats/exe_data.cpp` holds the import table: each piece's name (for text, its text ID),
segment:offset and length. The repo ships that table, never the text. A text is read the way the
printer reads it (docs/text.md): it ends at a NUL that is not a code's argument, or after a byte
with the top bit set.

## Overrides

A text file of the same form layered on top (`chr --text FILE`) replaces any ID, for a translation
or a rewrite: `{"texts": [{"id": "walk.too_deep", "text": "..."}]}`. Other members, such as
"address", are ignored. Control codes are written as `\u00NN` and new lines as `\r`, as in the
imported file. The names in the map files have IDs too (`map1.place.1`, `map1.person.0`), so
they can be replaced the same way. A mod's font is another font file.
