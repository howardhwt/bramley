Bramley
=======

An Apple-inspired OS for the TrimUI Brick Hammer and Brick, forked from NextUI. It is free and
not for sale. It replaces NextUI's launcher, Settings and emulator, and keeps the stock ones
beside them. Bramley was called Brick Hammer Launcher until October 2026.

Install
-------
1. NextUI must already be installed on the card.
2. Copy the "Bramley.pak" folder into Tools/tg5040/ on the card.
3. On the device, open Tools, then Bramley. The new home screen appears.

Installing over Brick Hammer Launcher updates it in place. Afterwards the old
"Brick Hammer Launcher.pak" folder can be deleted.

Remove
------
Open the Bramley Pak again (it is in Apps). Stock NextUI comes back.

After a NextUI update the stock programs may return. Open the Pak again to reinstall.
If the new launcher ever crashes twice in a row on start, the stock launcher opens instead.

Buttons
-------
Home:        D-pad moves, A opens or plays, X resumes a manual save.
Library:     L1 and R1 jump A to Z, B goes back.
Anywhere:    MENU opens the quick menu (Wi-Fi, Bluetooth, Settings, Sleep, Restart, Shut Down).
             SELECT opens Now Playing.
Now Playing: A play or pause, L1 and R1 track (chapter for books), left and right 10 seconds,
             Y play mode (speed for books), X locks the screen, B closes.
Lock screen: while music plays, a short power press turns the screen off and the music keeps
             going. Power again shows the lock screen, and power once more unlocks. Holding L3
             and R3 for a second also locks and unlocks. Holding power turns the device off.

Music and audiobooks
--------------------
Music/<Artist>/<Album>/<tracks>     or   Music/<Album>/<tracks>
Audiobooks/<Author>/<Book>/<files>  or   Audiobooks/<Book>/<files>
A cover.jpg or folder.jpg in the album folder is used as its cover; otherwise the picture inside
the first track, if it has one. Starting a game pauses the music; SELECT resumes it afterwards.

Collections
-----------
NextUI's Collections/<name>.txt files appear as a Collections row, in their own order.

Box art and console icons
-------------------------
Game box art:   Roms/<Console>/.media/<game name>.png   (the same as stock NextUI)
Console icons:  .media/consoles/<TAG>.png at the card root, where TAG is the short name in the
                console folder's brackets, for example GBA for "Game Boy Advance (GBA)".

Fonts
-----
Inter is included. To use another font you own, put its files in Fonts/ at the card root, with
Regular, SemiBold (or Medium) and Bold in the file names. Free fonts that look much like Apple's
system font include Inter Display, Manrope and Public Sans. Chinese, Japanese and Korean text
uses Source Han Sans from the device's own firmware.

Licenses
--------
NextUI is PolyForm Noncommercial 1.0.0 (LICENSE-NextUI.txt), so Bramley is free only. The Muse
player is from TortOS (MIT), Inter is SIL Open Font License 1.1, and three Settings symbols are
from Phosphor Icons (MIT); see THIRD-PARTY-LICENSES.md.
