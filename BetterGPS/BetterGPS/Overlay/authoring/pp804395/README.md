# Pana-Pacific PP804395 - 3D reconstruction for ATS 1.61

This folder is an authoring source for the rear-view camera kit, not a claim
that ATS already provides its camera feed or a stock mounting point.

## Texture authoring kit

The dedicated UV-unwrapped painting project, PNG templates, masks, layered
OpenRaster source and French instructions are in [texture-kit](texture-kit/LIRE-MOI.md).
Use that copy for texturing. The original reconstruction files below are kept
unchanged as the pre-UV source.

`pp804395_reconstruction.blend` and `pp804395_reconstruction.glb` are generated
with `scripts/create_pp804395_camera.py`. The model uses metres and faces
negative Y. The preview is a neutral studio render only.

## Evidence retained in the model

- PP804395 is the **rear** Pana-Pacific camera kit, supplied with the RRC-70
  70-foot cable; the SmartNav 2.0 guide identifies it as the back-up camera.
- The vendor lists a 1/3-inch colour CMOS sensor, a waterproof microphone,
  adjustable U-mount, sun shield, waterproof cable connection, and 18 IR LEDs.

## Dimensions

Pana-Pacific's public PP804395 listing and the SmartNav guide do **not** publish
an enclosure drawing. Therefore `pp804395_dimensions.json` marks every exterior
millimetre dimension as a photograph-proportioned reconstruction, rather than
an OEM measurement. It is a usable baseline for an ATS asset, but should be
replaced if an OEM mechanical drawing or a physical measurement becomes
available.

Before shipping to ATS, import the GLB into the version-matched SCS Blender
Tools project, assign the intended SCS material, make the vehicle-specific
mounting/LOD and export through that toolchain. Do not copy this generic model
over an existing SCS PMD/PMG or assume a functional reverse-video feed.
