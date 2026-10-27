# Home, Notrip and navigation

Each managed device keeps its own startup Home enable flag, visible duration
(default 2 seconds) and shortcut in `BetterGPSDevices.ini`. No placement,
camera, skin, startup or power setting is reset. Obsolete `page_mode` and
`idle_seconds` keys are accepted but ignored; new saves use `boot_home=1`
and `boot_home_seconds=2`. Zero duration or disabling the feature skips boot Home.

- Home is unavailable until a player-actor initialization is confirmed.
- A confirmed initialization arms one consumable startup step per device.
- Power-on retains the existing lockscreen/swipe or splash, then Home, then
  Nav if a route exists at that moment, otherwise Notrip. An existing route
  never skips this initial Home step. F7/towing uses the same actor lifecycle.
- Dwell starts only after a matching, uncovered Home frame is submitted by
  the actual display path. Preparing a CPU image does not start the counter.
- Movement, steering, pedals and keyboard/mouse activity never change pages.
  There is no idle return to Home, including at a red light.
- Consumed startup steps are not rearmed by menus, ignition, device power
  cycles, a route change, map textures or SDK timer restarts.
- The manual shortcut switches Home/Map after initialization, including during
  an active route. Manual Home has no timeout and overrides automatic navigation
  until the next explicit Map/toggle request. It shares the touch action's priority.
- The shortcut acts on the selected matching device, or the first eligible
  device when none is selected. Matching requires the exact modifiers; F9 stays
  latched until released, so releasing Ctrl/Shift first cannot activate power/menu.
- Pause and hidden startup/power-off freeze Home dwell. Animation clocks stay
  on the existing preparation worker, without another thread or input hooks.

The Display category exposes Home at first startup and Startup Home duration; Shortcuts
exposes Home / map shortcut. Reserved chord conflicts are rejected. A legacy
power/menu binding already using the default page chord is retained, with the
conflicting page binding disabled until reassigned.

## Initialization evidence and guards

The ATS 1.61 Windows x64 observer wraps the native player-actor initializer
(RVA 0x5d8b90, nine integer/pointer arguments forwarded unchanged). Resolution
checks the entry bytes, the game-control actor-slot use at 0x4d6838, the native
call site at 0x4d691b and the success log identity. The controller global is
0x36bad90, actor slot +0x31b0, actor's bound truck +0x18. Only the current player
binding is accepted after the original call; null requests and other actors
(including previews) are ignored. The observer never changes engine memory.

A candidate is not itself a spawn token. The SDK frame-end confirms the same
actor/truck after STARTED and fresh valid placement + speed. Duplicate telemetry
cannot rearm; another initialization of the same addresses can. A pending
initialization disables warm-resume's stale-channel shortcut. Unsupported code
signatures disable automatic Home only, without guessing from timers/ignition
or affecting existing navigation. Shutdown removes the observer.

Diagnostics: `[VehicleStart] Player initialization observer installed`, then
`[VehicleStart] Confirmed player initialization generation=...`. These events
must be checked in game for save loading/reloading, garage exits with/without
changes, F7 tow, dealership previews, menus with engine off, map, pause and
power cycling. Synthetic tests and executable guards do not establish that
all game-version-specific garage paths have been visually tested.

## Devices and resources

F150/Bronco keep their native CarPlay page trees. No new page policy is applied
to those displays. The existing lockscreen/swipe and Garmin splash durations
are unchanged.

Phones reuse their own selected lockscreen background, but never its Toplayer
on Home. There is one phone icon set (`iphone_lock_*` and `iphone_home_*`);
Android artwork and OS switching were retired on 2026-10-20. The two independent
clock typography options remain, now named Style 1 and Style 2. Old Android
icon preferences migrate to the retained icons without resetting the clock,
wallpaper, foreground or another device's preferences. Legacy `android` clock
font values remain readable; new saves use `style1`/`style2`.
Home status time is the live game clock, in white. Both Home and the lockscreen
retain 2/3 of background brightness at night (previously 1/2).
Resources are separated into
`material/ui/phone_lockscreen` and `material/ui/phone_home`.

Opening from Home expands the selected app's launch artwork from its icon
over 350 ms, holds it fullscreen for 1000 ms, then crossfades over the live
application for 300 ms. Other Notrip/Nav changes do not replay launch. Closing
to Home shrinks the actual completed map + UI snapshot, without the launch
logo, over 350 ms. GPU-only capture/composition occurs at Present using the
existing isolated/deferred context; it never reads back the map or injects a
draw in the game's resource binding. Private resources are tagged and released
with the existing phone surface.

Motion reference: the user's `2026-10-19 15.34.52.mp4` and Apple's
[Motion guidelines](https://developer.apple.com/design/human-interface-guidelines/motion).
Durations above are BetterGPS choices, not claimed Apple system timings.

The Sony Crown preset uses `material/sony/crown_victoria/sonygpshome.dds` with
the **live game time**, 12-hour numerals and smaller bold AM/PM. Its frame skin
is not painted on Home. This is a presentation choice, not an Add Display
recognition rule; phones on the same vehicle retain their phone presentation.

Nuvi and W900 former Notrip artwork is now under `home` / `home_kenworth1440`.
New Notrip pages keep the map, speed, speed limit and zoom, but omit guidance
headers and arrival text. W900 also retains its bottom frame/compass. Both
day/night variants and existing editable module slots are preserved.

Phones without a managed profile are adopted only after both their existing
physical front-surface marker and their existing native candidate are observed.
The currently playing startup phase is adopted, not restarted. The scanner,
shared texture identity and Add Display matching are unchanged. Explicit
Original/disabled choices take precedence.

## Validation

`OverlayPreviewTest <output> --device-pages` tests player/preview binding and
native argument forwarding, consumable generations, visible dwell, startup route
priority, hidden startup, pause/power-off, manual toggling, exact latched modifiers,
independent saved preferences, phone art/dim, live Sony AM/PM, both Garmin
formats/day-night and actual native map + UI closing with WARP in both deferred
and SINGLETHREADED isolated contexts. Existing device, native icon/map, startup,
F150/Bronco and install-preservation fixtures must also pass. These tests do
not replace an in-game visual check on the user's vehicles.
The closing case also suspends the map producer and leaves the prepared Home
unchanged across consecutive frames, checking that it cannot flicker to Home
before the animation ends.
