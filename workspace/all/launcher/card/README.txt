Bramley
=======

An Apple-inspired OS for the TrimUI Brick Hammer and Brick, forked from NextUI. It is free and
not for sale.

Install
-------
1. Use a card formatted as FAT32.
2. Extract everything in this zip to the top of the card, keeping the folders as they are.
3. Put the card in the device and turn it on. "Setting up Bramley" shows for a minute, then the
   home screen appears.

The first start also sets the device to start from the card from now on. TrimUI's own start-up
script is kept on the device beside it.

Update
------
Put the new MinUI.zip (released as Bramley-<version>-update.zip, renamed to MinUI.zip) at the
top of the card and restart the device. "Updating Bramley" shows, and saves, games, music and
settings stay as they are.

Back to TrimUI's own system
---------------------------
Delete the .tmp_update folder from the top of the card. The device then starts TrimUI's own
system again. Putting the folder back returns to Bramley.

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

Settings and apps
-----------------
Settings has Battery (level over time and time left), Lights, Date & Time (a list of cities for
the time zone) and Buttons, Test Buttons. Files browses the card, with a Trash that keeps deleted
files until it is emptied. Activity shows time played per day and per game.

Music and audiobooks
--------------------
Music/<Artist>/<Album>/<tracks>     or   Music/<Album>/<tracks>
Audiobooks/<Author>/<Book>/<files>  or   Audiobooks/<Book>/<files>
A cover.jpg or folder.jpg in the album folder is used as its cover; otherwise the picture inside
the first track, if it has one. Starting a game pauses the music; SELECT resumes it afterwards.

Collections
-----------
Collections/<name>.txt files at the top of the card appear as a Collections row, in their own order.

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
NextUI is PolyForm Noncommercial 1.0.0 (LICENSE), so Bramley is free only. The Muse
player is from TortOS (MIT), Inter is SIL Open Font License 1.1, and three Settings symbols are
from Phosphor Icons (MIT); see THIRD-PARTY-LICENSES.md.
The emulator cores keep their own licenses, mostly the GNU GPL; each core's license is in the
Licenses folder, and CORES-SOURCE.md lists the exact source each core is built from.
