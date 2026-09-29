repo: autotel/autotel-plugin-presets-librarian
branch: main
path: Source/

## Last sync

date: 2026-09-22T16:20:00Z

### Updated in this project

- Redesigned the librarian panel: tag rail with counts, tagged rows, row-anchored edit popover.
- Replaced the persistent right-hand details column with a popover on the selected row.
- Added a migrate-folder flow (choose destination, move presets, ask about deleting the old folder).
- Load now passes through a dirty guard instead of loading straight from a double-click.

## Screen map

| Project screen | Repo files |
| --- | --- |
| Presets Librarian.dc.html | Source/PresetPanel.h, Source/PresetPanel.cpp, Source/PresetLibrary.h, Source/PresetFile.h, README.md |
