# Wii U Menu Sort

:warning: **WARNING**: If you're not sure try on rednand first. I am not responsible for bricked consoles.

> That said, I tried 10 sorts on 2 accounts on USA sysNAND without issue.
> @Yardape8000

### Description

This will allow you to alphabetically sort your icons on the Wii U Menu.
Icons in folders are also sorted.
Sorts are done per user account.
Not much is shown for a UI. It will just start sorting and tell you when it is done in 5-10s.

The following items will not move:
Folders and system icons (Disc, Settings, etc)
Homebrew Launcher
CHBC (untested)
Any IDs specified in [dontmove.txt](dontmove.txt).

### Sorting modes

1. _Standard sorting_: sorts titles alphabetically.
2. _Standard sorting(ignoring leading 'The')_: sorts titles alphabetically, ignoring a leading 'The' in the titles name.
3. _Bad naming mode sorting_: sorts titles alphabetically by group names(see _Titles map list_). 
4. _Bad naming mode sorting(ignoring leading 'The')_: sorts titles alphabetically by group names(see _Titles map list_), ignoring a leading 'The' in the titles name.

You can try different sorting modes to see what works for you. Every time the app is executed it'll sort all titles based on the selected sorting mode.

### Don't move list

You can override the default [dontmove.txt](dontmove.txt) with your custom list.
Only use the last 4 bytes of the title ID.
The included sample file has:

```
10179B00 # Brain Age: Train Your Brain In Minutes A Day
10105A00 # Netflix
10105700 # YouTube
```

The file can be as large or small as you want, depending on the IDs you don't want to move.

You can also use <pre>dontmove<b>X</b>.txt</pre> where **`X`** is `0-9`, or `A` or `B`.
This allows each user to have separate selection of non-movable icons.
This is the last digit of the `8000000X` save folder used by the current user.
Shown by the program as User ID: `X`

`dontmoveX.txt` takes priority over `dontmove.txt`.
Only 1 file is used. The IDs are not merged.
You can delete the files if not needed.

**NOTE**: Don't forget to insert newline at the end of the `dontmove.txt` file.

### Titles map

You can override the default [titlesmap.psv](titlesmap.psv) with your custom map.
The file format is the following:
```
titleId#1|groupName
titleId#2|groupName
titleId#3|groupName
...
titleId#N|groupName
```

The included map has `6951` titles matched in groups. When _Bad naming mode sorting_ is chosen the title will be sorted based on the group and title name. If title is missing from the map it will sort it by name only.

The file can be as large or small as you want.

You can also use <pre>titlesmap<b>X</b>.psv</pre> where **`X`** is `0-9`, or `A` or `B`.
This allows each user to have separate selection of title maps.
This is the last digit of the `8000000X` save folder used by the current user.
Shown by the program as User ID: `X`

`titlesmapX.psv` takes priority over `titlesmap.psv`.
Only 1 file is used. The IDs are not merged.
You can delete the files if not needed.

### Build

You will need to compile the Wii U version of [libfat](https://github.com/dimok789/libfat).
make from the `wiiu\` directory.
Copy the headers and complied library to `portlibs\ppc\`.
