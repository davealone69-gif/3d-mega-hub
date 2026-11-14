# Filament and gltfio bind their native methods by class and method name (JNI). R8 must not rename
# or remove them, otherwise the release build fails at runtime with "No implementation found".
# The single rule below covers every Filament package, including gltfio.
-keep class com.google.android.filament.** { *; }
