# T680 NavPlus HD camera page

The T680 NG Home Camera Views tile opens a dedicated Camera page. The supplied
`navplushd_simple_background.dds` is opaque RGB(10,10,10); the supplied
`navplushd_camera_icon.dds` is fitted proportionally at the left of the app body
(153x294 in the 900x544 canvas, starting at x=0, y=126; 30% smaller and vertically
centred). The status bar, clock and
bottom controls remain shared with Home, Gauges and navigation.

This page retains the user-requested no-camera placeholder: the selector
is static and no camera feed, map or navigation UI is drawn. This is not proof
that the T680 family has no cameras: stock DigitalVision mirror accessories
are confirmed in [the 25 October audit](truck-camera-audit-20261025.md). The
actual accessory selection and any future video connection must be checked separately. There is no
blinker/reverse-triggered page change.

Every launch from Home shows the supplied `navplushd_camera_noavailable.dds`
after the opening swipe. The card is centred in the app body at (263,234),
374x78 pixels (about 42% of screen width), matching the user's sizing reference.
Only its lower OK row dismisses it; it has no timer and ignores outside taps,
Home, Return and the page shortcut while open. Dismissal changes the prepared
render key without navigating away. Temperature warnings remain pending until
OK is acknowledged. The popup is not reopened when returning from a warning.

Opening uses the existing 500ms click latency and 320ms swipe without the
navigation-only CoPilot loading logo. Return, Home and the Home shortcut close
the page with the existing 250ms click latency and 320ms swipe. Route changes
do not dismiss Camera. Gauge temperature warnings retain page history, allowing
Return to restore Camera before returning Home.

`scripts/import_copilot_camera.py` imports only the three artwork files as
uncompressed DDS and verifies decoded pixel/alpha equality. The camera fixture
checks opaque RGB10 coverage, selector placement, touch, transitions and page
isolation; existing gauge and page regression fixtures remain applicable.
