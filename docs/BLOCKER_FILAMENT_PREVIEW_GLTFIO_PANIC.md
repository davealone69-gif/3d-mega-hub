# Blocker: Filament preview aborts the process on GLB load

**Status: BLOCKED. The native abort is observed; its root cause is not yet proven.**
**Affected feature: the Filament live preview — and therefore animation controls, since only the
Filament path can play imported glTF clips.**

## What happens

On an x86_64 API 30 emulator (ANGLE/SwiftShader, Filament feature level 1) the app dies the moment the
Filament preview loads a GLB. The sequence, from run `44134945773`:

```
17:28:23.857 MegaHubRenderer: Renderer status: Filament and gltfio JNI loaded; initializing OpenGL ES first
17:28:23.857 libc++abi: terminating due to uncaught exception of type N5utils17PreconditionPanicE: Precondition
17:28:23.857 libc++abi: in getFieldInfo:175
17:28:23.857 libc++abi: reason: uniform named "sheenColorIndex" not found
17:28:23.857 libc: Fatal signal 6 (SIGABRT) in tid 6280 (om.davemega.hub)
17:28:24.182 ActivityManager: Process com.davemega.hub has died: fg TOP
```

Tombstone `tombstone_00`, thread `om.davemega.hub`, on the same run:

```
Abort message: 'terminating due to uncaught exception of type N5utils17PreconditionPanicE: Precondition
in getFieldInfo:175
#02..#10   libfilament-jni.so   (BuildId 88dacb49…)
#11..#14   libgltfio-jni.so     (BuildId 67d04d19…)
#15        filament::gltfio::AssetLoader::createAsset(unsigned char const*, unsigned int)+14
#16        Java_com_google_android_filament_gltfio_AssetLoader_nCreateAsset+46
#24        com.google.android.filament.gltfio.AssetLoader.createAsset+12
```

So the failure is: `AssetLoader.createAsset` → gltfio builds the asset → gltfio sets a material uniform
named `sheenColorIndex` → the material instance in use does not declare it → Filament's
`getFieldInfo` raises `PreconditionPanic` → nothing catches it → `abort()` → the whole app dies.

This is exactly the class of failure that a compiler cannot catch: the Java and JNI signatures are
unchanged between 1.71.6 and 1.80.1, so the code compiles and then aborts at runtime.

## Evidence and hypotheses (do not treat the hypothesis as a diagnosis)

- The app uses the stock `UbershaderProvider(engine)` and the stock
  `AssetLoader(engine, provider, EntityManager.get())`; no custom material package, no custom
  `MaterialProvider`, no modified gltfio.
- The three artifacts are version-aligned through the Gradle version catalog. The failing run described below used 1.80.1. The current PR branch pins 1.79.0 as a rollback experiment; it has not yet been runtime-tested.
- The asset is structurally valid. `models/female.glb` was validated offline: GLB magic and version 2,
  declared length equal to actual length, both chunks correctly sized, `asset.version` 2.0, one buffer
  with no URI whose `byteLength` fits the BIN chunk, 11 accessors each with `componentType`/`count`/
  `type` and in-range `bufferView`, meshes with POSITION, no dangling indices, **no extensions**, and
  no sheen material — yet gltfio still touches a sheen uniform.
- The same failure is reported upstream against other Filament consumers using `AssetLoader`
  ([google/filament#6907](https://github.com/google/filament/issues/6907): "Panic in getFieldInfo:170
  reason: uniform named \"sheenColorIndex\" not found"), where it was closed as the caller's problem
  because the material variant did not match the asset. In our case the caller *is* the standard
  `UbershaderProvider`, so the variant/material mismatch happens inside Filament.
- The emulator's engine reports **`Feature level: 1`**. Filament strips material features at lower
  feature levels, which is consistent with a ubershader variant that has no sheen uniforms while the
  loader still writes them.

## Checks already performed (limited scope)

| Hypothesis | Verdict |
| --- | --- |
| Heap `ByteBuffer` passed to `createAsset` | **Not currently indicated.** Filament's `AutoBuffer` copies non-direct buffers; this does not rule out every input/lifetime issue. |
| Malformed GLB / bad accessors / dangling indices | The bundled fixture passed the documented offline structural checks. This does not prove every runtime/material condition is valid. |
| Missing/duplicated Filament engine, wrong lifecycle | The captured run shows one engine creation and OpenGL backend selection. This does not fully rule out lifecycle or thread-ownership defects. |
| R8/minification stripping Filament resources | **Not the cause.** The crash reproduces in the debug build. |
| Our own GLES renderer | Separate path. It passed the generated-GLB test suite on a prior exact CI commit; that is not a test of this Filament preview path. |

## Options, in order of preference

1. **Verify on hardware before changing anything.** The SM-S921B reports OpenGL ES 3.2 and should reach
   Filament feature level 2 or 3, where the ubershader variant may well declare the sheen uniforms. A
   single instrumented run on the phone decides whether this is emulator-only. Nothing in this
   workspace can substitute for that run.
2. **Test the current 1.79.0 pin** as a controlled rollback experiment. Only a dedicated imported-GLB preview test on the exact current commit can establish whether it changes the result. A pass would support the rollback; it would not by itself prove the upstream root cause.
3. **Supply an explicit `MaterialProvider`** built from materials we compile ourselves with the exact
   feature set we need, so the uniforms gltfio writes are guaranteed to exist. This is the durable fix
   if the upstream path cannot be trusted, and it also gives us control over the toon/PBR look.
4. **Report upstream** with the tombstone, the asset and the feature level — this is a reproducible
   `AssetLoader` abort on a valid asset.

Do not paper over this by removing the preview, by catching something uncatchable, or by declaring the
preview verified because the app compiles.

## What still works without the preview

The production rendering path is not affected. The app renders imported and generated GLBs through
`AvatarForgeSurfaceView` (native OpenGL ES), which is verified on the emulator down to pixel counts.
The consequence is specifically that **imported skeletal animation cannot be exercised**, because clip
playback depends on the Filament path.


## Current branch handover — 2026-11-19

The active PR #14 branch pins Filament 1.79.0. This is an experiment only: the latest PR head has no Actions run or status check, and no new imported-GLB Filament runtime result is available. The prior 1.80.1 native abort remains a valid historical observation, not proof that 1.79.0 fails or passes. Keep the feature labelled **BLOCKED / UNVERIFIED** until an exact-head test exercises the imported GLB through `FilamentPreviewView` and captures process/logcat evidence. The native AvatarForge GLES path is separate and must remain the default production route.

### Capability-level handling added 2026-11-19

The current branch now queries `Engine.getSupportedFeatureLevel()` after creating the selected backend and calls `setActiveFeatureLevel()` with that reported supported level before constructing `UbershaderProvider` / gltfio loaders. Filament's default is feature level 1 even on some GLES 3.1+ contexts; this change avoids silently retaining that default when the backend reports a higher supported level. It does not force unsupported capabilities. **Runtime result: UNVERIFIED.** This is a grounded capability-selection correction, not proof that feature level caused the missing-uniform panic or that the panic is fixed.
