# Native rear reference within the current four-view NavPlus app

The user rejected an attempted rollback to the old DLL/reverse-only proof.
That rollback was immediately undone with its verified backup. Do not deploy
an older DLL or disable the T680 managed display to compare cameras again.

Confirmed differences: the successful original test changed the cam_parkbck
locator in three cab PMGs, then let SCS initialize and update its camera from
those poses. The managed implementation instead initialized a placeholder,
rewrote internal poses, then corrected the exported render quaternion. The
export audit measured its angles/position but not the truck's geometry visibility.
A separate source-camera frustum builder reads the source pose; proving the
export quaternion alone does not prove the complete native render state.
The exact cause of the missing chassis is not yet demonstrated by a game A/B.

This build retains the current DLL, four selections, triggers, Home and touch.
It promotes ONLY the three exact PMGs from the validated proof (pinned hashes),
updates their hardware manifest, and changes Reverse to an unmodified native
path inside the Camera app. It restores the saved SCS inputs once on entering
Rear, leaves them alone on subsequent frames, and bypasses final-quaternion
correction for that source. Switching to Forward/Left/Right still uses the
managed path. Closing restores the original native fields.

The rear-only generated UI variant uses the complete material with the same
UVs as the proof (left=0,right=1,top=1,bottom=0), fitted into the app body.
The other three views keep their existing X correction. This makes the rear
comparison include the original full source field, not the cropped UV region.
Source ownership now uses the plugin's unique cabin tag plus the verified
native producer type, not the removed placeholder coordinates.

Audit path SCS_VALIDATED_REAR_UNMODIFIED_U0_V1 identifies the comparison.
It records reverseGear and the native mask BEFORE forced activation. Compare
Rear while actually in reverse: does the chassis reappear? If so, late pose
mutation is implicated; if not, UI/activation/visibility state remains distinct
from the original proof. Do not claim the missing-body cause is proven from
zero quaternion errors. For this path the raw native quaternion is logged as
NATIVE_REFERENCE_RAW, without pretending it is a reconstructed display basis.

Regression coverage: native inputs restored exactly once, source switching
retained, three original cab files matched to the proof hashes, native rear
UV graph, Camera/Home/touch isolation and native-fork. Tests pass in
20261026T232512Z-b5315894. Current build 20261026T232331Z-deb8bfc6.
