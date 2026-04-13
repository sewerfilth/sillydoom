sillydoom — DOOM on Nintendo Switch
====================================

Installation:
  1. Copy the "switch" folder to your SD card root
     so you have: sdmc:/switch/sillydoom/sillydoom.nro

  2. Place your DOOM WAD file in the same folder:
     sdmc:/switch/sillydoom/DOOM.WAD
     (supports DOOM.WAD, DOOM1.WAD, DOOM2.WAD)

  3. Launch from the Homebrew Menu.

Controls:
  L-stick            Move forward/back + strafe
  R-stick            Turn left/right
  D-pad              Move + turn (digital)
  A                  Use / Open / Menu select
  B                  Run (sprint)
  X                  Automap
  Y                  Menu confirm (yes)
  ZR                 Fire
  ZL                 Use (alt)
  L / R shoulders    Strafe left/right
  +                  Menu (ESC)
  -                  Automap (TAB)

Building:
  Default:   make                  (small NRO, WAD loaded from SD card)
  Embedded:  make EMBED_WAD=1      (large NRO, WAD in romfs)

Notes:
  - WAD is loaded from sdmc:/switch/sillydoom/ first,
    then falls back to embedded romfs
  - Config saves to sdmc:/switch/sillydoom/.doomrc
  - Supports DOOM Shareware (DOOM1.WAD), Registered
    (DOOM.WAD), and DOOM II (DOOM2.WAD)

Version: 0.2.0
