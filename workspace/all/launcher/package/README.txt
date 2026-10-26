Brick Hammer Launcher
=====================

An Apple TV style home screen for NextUI on the TrimUI Brick Hammer and Brick. It is free and
not for sale. It replaces only NextUI's launcher: games, emulators, Settings and Paks are stock.

Install
-------
1. NextUI must already be installed on the card.
2. Copy the "Brick Hammer Launcher.pak" folder into Tools/tg5040/ on the card.
3. On the device, open Tools, then Brick Hammer Launcher. The new home screen appears.

Remove
------
Open the Brick Hammer Launcher Pak again (it is in Apps). The stock NextUI launcher comes back.

After a NextUI update the stock launcher may return. Open the Pak again to reinstall.
If the new launcher ever crashes twice in a row on start, the stock launcher opens instead.

Buttons
-------
Home:        D-pad moves, A opens or plays, X resumes a manual save.
Library:     L1 and R1 jump A to Z, B goes back.
Anywhere:    MENU opens the quick menu (Wi-Fi, Bluetooth, Settings, Sleep, Restart, Shut Down).
             SELECT opens Now Playing.
Now Playing: A play or pause, L1 and R1 track (chapter for books), left and right 10 seconds,
             Y play mode (speed for books), B closes.

Music and audiobooks
--------------------
Music/<Artist>/<Album>/<tracks>     or   Music/<Album>/<tracks>
Audiobooks/<Author>/<Book>/<files>  or   Audiobooks/<Book>/<files>
A cover.jpg or folder.jpg in the album folder is used as its cover; otherwise the picture inside
the first track, if it has one. Starting a game pauses the music; SELECT resumes it afterwards.

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
NextUI is PolyForm Noncommercial 1.0.0 (LICENSE-NextUI.txt), so this fork is free only. The Muse
player is from TortOS (MIT) and Inter is SIL Open Font License 1.1; see THIRD-PARTY-LICENSES.md.
