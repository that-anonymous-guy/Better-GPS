# Native camera research (current executable only; not an implemented ABI)

This is supporting evidence for the pending camera integration. Every address
below must be signature-checked against the live executable before use. None
of these notes alone proves working off-screen rendering.

Official SCS documentation identifies dashboard parking-camera groups 910-913:
https://modding.scssoft.com/wiki/Documentation/Engine/Truck_Interior_Animations_and_IDs

Static inspection of the local amtrucks.exe (2026-10-26) found:

- Camera locator table RVA 0x2d64380: cam_d, cam_ds, cam_p, cam_ps, cam_s,
  cam_f, cam_h, cam_parkbck, cam_park360. Digital mirror table 0x2d643d0.
- Dashboard owner+0x118 points to the truck/source object. Owner+0x13a8 holds
  the camera pointer array wrapper (+8 storage, +0x10 count). Slot 7 is Park.
  Getter at RVA 0x549c80, creation at 0x548ef0.
- Native Park producer is created at 256x256; Park360 uses 128x256.
  Creation function 0x4748d0. Owner+0x1658 initializes to -1.
  This field is used for texture resolution selection, not verified as an enable flag.
  Activation semantics still require verification (callers 0x533600, 0x548290,
  0x548820). Do not write this field based only on its initial value.
- Native dynamic reflection path formats include
  /material/environment/%s_mirror_reflection.tobj and
  /material/environment/mirror/%s_%ux%u_ui.mat.
  They are engine aliases, not missing physical stock files to invent.
- Installed scene-locator search at 0x549a10 uses truck+0x798/0x7a0 entries
  (0x30 stride, node at +0x10), query helper 0x15e4de0, and recursive truck
  virtual function at vtable+0x118 for nested parking-camera locators.
- Observed query call: RCX scene node, RDX output pose32, R8 packed-name pointer.
  Recursive call: RCX truck, RDX pose32, R8 name pointer; returns scene node.
- Node+0x30 is parent pose32. Compose at 0x139d890 receives parent, output,
  local pose in RCX/RDX/R8. Pose32 is float3 position, two int16 sector coordinates, then WXYZ quaternion float4.
  Use the existing guarded native unpack helper for world-space positions.
- Camera unit factory 0x87f760 allocates 0x548 bytes. Pose fields observed at
  +0x480, +0x4a0, +0x4c0, +0x4e0. FOV/projection fields are not sufficiently
  verified; never write a guessed degree/radian value to +0x20.
- Existing mirrors must not be hijacked. Candidate plan: own one Park producer
  enabled only for Camera, resolve the actual installed optical locator, update
  its pose, compose into the display on GPU, retain low native resolution.
- A semantically tagged native UI probe could identify the Park SRV; generic
  256x256 texture interception is insufficient and must not be used.

Disassemblies and native_inspect.py are in build/camera-integration-20261026/.
They are local investigation artifacts, not an API guarantee or shipped code.
