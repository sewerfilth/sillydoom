# 07 — Controls

## Keyboard (macOS / Linux)

| Key | Action |
|-----|--------|
| W / Up Arrow | Move forward |
| S / Down Arrow | Move backward |
| A | Strafe left |
| D | Strafe right |
| Left Arrow | Turn left |
| Right Arrow | Turn right |
| Space | Use / Open doors |
| Ctrl | Fire weapon |
| Shift | Run (sprint) |
| Escape | Menu |
| Enter | Menu select |
| Tab | Automap |
| 1-7 | Select weapon |
| Y | Menu confirm (yes) |
| N | Menu deny (no) |
| F2 | Save game |
| F3 | Load game |
| F6 | Quicksave |
| F9 | Quickload |

## Mouse (macOS)

| Input | Action |
|-------|--------|
| Move X | Turn left/right |
| Left click | Fire |
| Right click | Strafe modifier (hold + move to strafe) |
| Middle click | Move forward |

Mouse is captured on launch (cursor hidden, raw delta input via
`CGGetLastMouseDelta`). Cursor is released on quit.

## Nintendo Switch

### Buttons

| Button | Action | DOOM Key |
|--------|--------|----------|
| A | Use / Open / Menu select | Space / Enter |
| B | Run (sprint) | KEY_RSHIFT (0xB6) |
| X | Automap | Tab |
| Y | Menu confirm | 'y' |
| ZR | Fire | KEY_RCTRL (0x9D) |
| ZL | Use (alt) | Space |
| L shoulder | Strafe left | ',' |
| R shoulder | Strafe right | '.' |
| + (Plus) | Menu | Escape |
| - (Minus) | Automap | Tab |

### Analog Sticks

| Stick | Action | Mapping |
|-------|--------|---------|
| L-stick up | Move forward | WASD 'W' → KEY_UPARROW |
| L-stick down | Move backward | WASD 'S' → KEY_DOWNARROW |
| L-stick left | Strafe left | WASD 'A' → ',' |
| L-stick right | Strafe right | WASD 'D' → '.' |
| R-stick left/right | Turn | ev_mouse dx (scale 30) |

### D-pad

| Direction | Action | DOOM Key |
|-----------|--------|----------|
| Up | Move forward | KEY_UPARROW (0xAD) |
| Down | Move backward | KEY_DOWNARROW (0xAF) |
| Left | Turn left | KEY_LEFTARROW (0xAC) |
| Right | Turn right | KEY_RIGHTARROW (0xAE) |

### Stick Calibration

- `hidSetNpadAnalogStickUseCenterClamp(true)` enabled at init
- L-stick: 30% activation threshold via `_left_stick_as_key()`
- R-stick: 20% deadzone, linear scaling to mouse delta

## Input Architecture

```
Physical input (button/stick/mouse)
    │
    ▼ Platform layer (platform_switch.c / Cocoa)
isilly keycode (ASCII for letters, JS-style for arrows)
    │
    ▼ poll_callback() in ext_doom_engine.c
post_key_if_changed(isilly_key, doom_key)
    │
    ├── Edge detection via g_prev_keys[256]
    ├── Posts ev_keydown on press, ev_keyup on release
    │
    ▼ doom_isilly_post_event()
DOOM event queue (64-entry ring buffer)
    │
    ▼ I_StartTic() drains queue
D_PostEvent() → DOOM event processing
```

Mouse/stick turning bypasses the key system and posts `ev_mouse` events
directly with `data2` (horizontal delta) for turning.
