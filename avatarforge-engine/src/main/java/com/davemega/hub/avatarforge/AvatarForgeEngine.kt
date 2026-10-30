package com.davemega.hub.avatarforge

import android.content.Context
import java.io.File
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.runInterruptible

data class AvatarForgeHealth(
    val nativeLibraryLoaded: Boolean,
    val abiVersion: Int? = null,
    val engineVersion: String? = null,
    val problem: String? = null,
) {
    val usable: Boolean get() = nativeLibraryLoaded && problem == null
}

data class AvatarGenerationResult(
    val file: File,
    val vertexCount: Int,
    val indexCount: Int,
    val triangleCount: Int,
    val submeshCount: Int,
    val jointCount: Int,
    val blendshapeCount: Int,
    val textureBytes: Long,
    val geometryBytes: Long,
    val peakFieldBytes: Long,
    val memoryBudgetBytes: Long,
    val promptMatchedAttributes: Int,
) {
    val glbBytes: Long get() = file.length()
    val promptMatchWarning: String?
        get() = if (promptMatchedAttributes == 0) {
            "AvatarForge's deterministic parser recognized no prompt attributes; review the defaults before using this avatar."
        } else null
}

/**
 * Renderer-independent host facade for the pinned AvatarForge C++ generation kernel.
 * Generation is a native, on-device operation; it is not run merely by opening the app.
 */
object AvatarForgeEngine {
    private const val EXPECTED_ABI_VERSION = 1
    private const val MIB = 1024L * 1024L
    const val DEFAULT_MEMORY_BUDGET_BYTES: Long = 1536L * MIB
    private const val MIN_MEMORY_BUDGET_BYTES = 512L * MIB

    private val cachedHealth: AvatarForgeHealth by lazy(LazyThreadSafetyMode.SYNCHRONIZED) {
        val load = runCatching { System.loadLibrary("mega_hub_avatarforge") }
        val loadError = load.exceptionOrNull()
        if (loadError != null) {
            AvatarForgeHealth(
                nativeLibraryLoaded = false,
                problem = "Native AvatarForge library could not load: ${loadError.message ?: loadError.javaClass.simpleName}",
            )
        } else {
            runCatching {
                val abi = NativeMethods.abiVersion()
                val version = NativeMethods.engineVersion()
                AvatarForgeHealth(
                    nativeLibraryLoaded = true,
                    abiVersion = abi,
                    engineVersion = version,
                    problem = if (abi != EXPECTED_ABI_VERSION) {
                        "AvatarForge ABI mismatch: app expects $EXPECTED_ABI_VERSION, native engine reports $abi"
                    } else null,
                )
            }.getOrElse { failure ->
                AvatarForgeHealth(
                    nativeLibraryLoaded = true,
                    problem = "AvatarForge native health check failed: ${failure.message ?: failure.javaClass.simpleName}",
                )
            }
        }
    }

    fun health(): AvatarForgeHealth = cachedHealth

    /**
     * Generates a GLB in app-private storage. The caller must keep the prompt/spec visible to the
     * user; resource checks are a conservative gate, not a promise that Android will not reclaim RAM.
     * Cancellation is cooperative at the kernel's callback safe points.
     */
    suspend fun generate(
        context: Context,
        spec: AvatarSpec,
        prompt: String = spec.prompt,
        outputName: String = "avatar-${spec.seed}-${System.currentTimeMillis()}.glb",
        requestedMemoryBudgetBytes: Long = DEFAULT_MEMORY_BUDGET_BYTES,
    ): AvatarGenerationResult = runInterruptible(Dispatchers.IO) {
        require(requestedMemoryBudgetBytes >= MIN_MEMORY_BUDGET_BYTES) {
            "AvatarForge generation budget must be at least 512 MiB"
        }
        val safePrompt = prompt.trim()
        require(safePrompt.isNotEmpty()) { "A non-empty avatar prompt is required" }
        require(safePrompt.length <= 500) { "Avatar prompt must be 500 characters or fewer" }
        spec.copy(prompt = safePrompt).requireGeneratable(safePrompt)

        val engineHealth = health()
        check(engineHealth.usable) {
            engineHealth.problem ?: "AvatarForge native engine is unavailable"
        }

        val appContext = context.applicationContext
        val resources = DeviceResourceGovernor(appContext).snapshot()
        check(!resources.generationShouldBeDeferred) {
            "AvatarForge generation deferred: ${resources.state}; available RAM=${resources.availableRamBytes} bytes, " +
                "free app storage=${resources.availableStorageBytes} bytes, thermal=${resources.thermalStatus ?: "unavailable"}"
        }
        val budget = minOf(requestedMemoryBudgetBytes, DEFAULT_MEMORY_BUDGET_BYTES, resources.availableRamBytes / 2)
        check(budget >= MIN_MEMORY_BUDGET_BYTES) {
            "AvatarForge generation deferred: measured RAM headroom only supports a ${budget / MIB} MiB engine budget"
        }

        require(outputName == File(outputName).name && outputName.endsWith(".glb", ignoreCase = true)) {
            "Output name must be a plain .glb filename inside app-private storage"
        }
        val directory = File(appContext.filesDir, "avatars")
        check(directory.isDirectory || directory.mkdirs()) { "Could not create the private avatar library directory" }
        val destination = File(directory, outputName)
        check(destination.createNewFile()) { "Avatar output already exists: $outputName" }

        val stats = try {
            NativeMethods.generate(
                ints = spec.packInts(),
                floats = spec.packFloats(),
                prompt = safePrompt,
                outputPath = destination.absolutePath,
                memoryBudgetBytes = budget,
                adultModeConfirmed = spec.adultModeConfirmed,
                cancellationToken = NativeCancellationToken(),
            ).also { result ->
                check(result.size == 10) { "AvatarForge returned ${result.size} result fields; expected 10" }
                check(destination.isFile && destination.length() > 0L) {
                    "AvatarForge reported success but no non-empty GLB was written"
                }
            }
        } catch (failure: Throwable) {
            destination.delete()
            throw failure
        }

        AvatarGenerationResult(
            file = destination,
            vertexCount = stats[0].toInt(),
            indexCount = stats[1].toInt(),
            triangleCount = stats[2].toInt(),
            submeshCount = stats[3].toInt(),
            jointCount = stats[4].toInt(),
            blendshapeCount = stats[5].toInt(),
            textureBytes = stats[6],
            geometryBytes = stats[7],
            peakFieldBytes = stats[8],
            memoryBudgetBytes = budget,
            promptMatchedAttributes = stats[9].toInt(),
        )
    }
}

private object NativeMethods {
    external fun abiVersion(): Int
    external fun engineVersion(): String
    external fun generate(
        ints: IntArray,
        floats: FloatArray,
        prompt: String,
        outputPath: String,
        memoryBudgetBytes: Long,
        adultModeConfirmed: Boolean,
        cancellationToken: NativeCancellationToken,
    ): LongArray
}

internal class NativeCancellationToken {
    fun shouldCancel(): Boolean = Thread.currentThread().isInterrupted
}
