# Character creator menu

The mobile home screen and Avatar Studio use a dark, compact character-creator design, informed by the visual references supplied by the owner. Scout is a ponytailed recon-style starting point; Sentinel is a plate-armored starting point. Both are **procedural presets**, not copies of the reference characters or bundled finished GLBs.

## Using the editor

1. Tap **Create Scout** on the home screen, or open **Avatar** and tap **Browse character presets**.
2. Select Scout or Sentinel. Selection changes the draft only; it does not replace the currently selected model.
3. Adjust body, build, height, hair style/color, skin tone, outfit and footwear. These values map to existing AvatarForge `AvatarSpec` fields. All exposed outfits are clothed.
4. Tap **Generate Scout/Sentinel** or **Generate with these settings**. Generation runs on-device, checks available resources and can be cancelled. Keep the studio open until it completes. On success the validated GLB is selected for the existing viewport; on failure or cancellation the previous model remains selected.
5. The free-text prompt remains available below the viewport. Editing it clears the preset selection and uses the existing prompt-based defaults instead of silently applying hidden preset settings.

The draft survives navigation within the app and Android saved-state restoration. It is not a named, permanent character library. Imported GLBs, the sample model and renderer diagnostics remain available.

## Animation buttons

Below the viewport, **Idle**, **Walk**, **Run** and **Wave** are shortcuts to playable clips with those names in the current model. The **All clips** row exposes the other embedded animations, including clips in imported GLBs with different names. Play/pause, stop/reset, previous/next, scrubbing, loop and speed controls operate on the selected clip. Clip names and original indices come from the loaded model, not from an assumed order; clips with zero or invalid duration cannot be played.

The bundled sample GLB is static and has no clips, so its animation buttons are disabled with an explanation. AvatarForge-generated characters include built-in animation clips, or you can import a GLB that already contains animation. Use the **Filament** preview: the alternative AvatarForge GLES preview is static. Animated models and the controls still need emulator and physical-device verification.

Switching to a different clip reloads the GLB before applying the selected animation. This uses a fresh Filament asset to reset its internal translation, rotation, scale and morph-weight state, rather than restoring node matrices alone. Large models can take time to reload; the controls show a loading state during the switch. Same-node and morph-switch behavior is covered by the Android smoke test, but still needs emulator and physical-device verification.

## What is and is not implemented

- The cards and home banner contain AI-generated **concept art**. They are not screenshots of the actual generated 3D models.
- Character appearance is approximate: the existing engine supports generic hairstyles, bodies, clothing and armor, not reference-image reconstruction, the precise pictured face, masks/helmets, bows, weapons or tactical attachments.
- Editing parameters does not live-edit an existing GLB. It creates a new model when generation is requested.
- The existing resource, thermal, adult-mode, validation and renderer gates still apply. A successful edit or native parser test is not a visual-quality or physical-device verification.

## Verification

`CharacterPresetsTest` covers draft selection, safe clothed presets, parameter mapping and stale/edited-prompt fallback. The host C++ `character_preset_prompt_regression` checks that the native prompt parser retains the presets' intended settings across 700 combinations of the exposed body, build, hair, outfit and footwear options. `AnimationPlaybackTest` covers clip lookup, invalid durations and navigation. `CharacterMenuSmokeTest` covers navigation and confirms that merely selecting a preset does not replace the current model; `CharacterAnimationSmokeTest` exercises real clip selection and a node transform with a small animated GLB fixture. Run Android unit/build/instrumentation tests with the commands in [BUILD_AND_VERIFY.md](../BUILD_AND_VERIFY.md); emulator and physical-phone results must be collected separately.

On 2026-11-18, all seven host CTest regressions passed, as did nine preset/animation JUnit tests using a locally source-built JUnit. Host-generated Scout and Sentinel GLBs passed the app's GLB validator and each contained 15 animation clips. Those host outputs were about 82 MiB and 90 MiB respectively; device memory use and playback performance remain unverified. Local Gradle download was blocked, but GitHub Actions built the debug APK and passed unit, lint and R8 checks at commit `d0f2f65`. Its emulator run confirmed same-node transform restoration, but failed on a disabled-control accessibility assertion and a partially clipped viewport capture; both have been updated and require a rerun. Morph-switch and physical-phone verification remain outstanding.
