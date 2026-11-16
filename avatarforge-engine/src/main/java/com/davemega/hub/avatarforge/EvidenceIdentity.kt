package com.davemega.hub.avatarforge

import android.content.Context
import android.content.pm.ApplicationInfo
import android.os.Build
import androidx.core.content.pm.PackageInfoCompat
import java.io.File
import java.security.MessageDigest
import java.util.concurrent.ConcurrentHashMap
import org.json.JSONObject

/**
 * Identity of the installed build, the device and a rendered asset.
 *
 * Every saved renderer report carries it, so a report can be tied to one APK, one device and one
 * file without relying on anyone's memory of what was installed.
 *
 * The APK digest is the SHA-256 of the installed base APK. An APK that is installed directly
 * (adb install, or opening the file on the phone) is stored unchanged, so the digest equals the
 * digest of the downloaded file. An install from an app bundle is split into several APKs;
 * `installedFromSplits` then says that the digest cannot be compared with a published APK.
 *
 * Hashing reads whole files. [build], [assetSha256] and [summaryLines] must not be called on the
 * UI thread; [buildIfReady] and [assetSha256IfReady] return cached values only and never block.
 */
object EvidenceIdentity {
    private val buildLock = Any()
    @Volatile private var installedBuildJson: String? = null
    private val assetDigests = ConcurrentHashMap<String, String>()

    /** Build and device identity. The installed APK is hashed once per process. */
    fun build(context: Context): JSONObject {
        val cached = installedBuildJson
        if (cached != null) return JSONObject(cached)
        val described: String = synchronized(buildLock) {
            val existing = installedBuildJson
            if (existing != null) {
                existing
            } else {
                val text = describeInstalledBuild(context.applicationContext).toString()
                installedBuildJson = text
                text
            }
        }
        return JSONObject(described)
    }

    /** The cached identity, or null when [build] has not run in this process yet. */
    fun buildIfReady(): JSONObject? {
        val cached = installedBuildJson ?: return null
        return JSONObject(cached)
    }

    /**
     * SHA-256 of [file] in lower-case hex, cached per path, length and modification time.
     * Empty when the file is missing or cannot be read.
     */
    fun assetSha256(file: File): String {
        val key = assetKey(file) ?: return ""
        assetDigests[key]?.let { return it }
        val digest = runCatching { sha256(file) }.getOrDefault("")
        if (digest.isNotEmpty()) assetDigests[key] = digest
        return digest
    }

    /** The cached digest of [file], or an empty string when [assetSha256] has not run for it. */
    fun assetSha256IfReady(file: File): String {
        val key = assetKey(file) ?: return ""
        return assetDigests[key] ?: ""
    }

    /** Lines for the on-screen diagnostics panel. */
    fun summaryLines(context: Context, asset: File): List<String> {
        val build = build(context)
        val buildType = if (build.optBoolean("debuggable")) "debug" else "release"
        val apkDigest = build.optString("apkSha256")
        val splitNote = if (build.optBoolean("installedFromSplits")) {
            " (split install: not comparable with the published APK)"
        } else {
            ""
        }
        val assetDigest = assetSha256(asset)
        return listOf(
            "Build ${build.optString("versionName")} (${build.optLong("versionCode")}) · $buildType · " +
                "${build.optString("deviceManufacturer")} ${build.optString("deviceModel")} · " +
                "Android ${build.optString("androidRelease")} (API ${build.optInt("androidSdk")})",
            "Installed APK SHA-256 " + (if (apkDigest.isEmpty()) "not available" else apkDigest) + splitNote,
            "Asset SHA-256 " + (if (assetDigest.isEmpty()) "not available" else assetDigest)
        )
    }

    /** SHA-256 of a file in lower-case hex. */
    fun sha256(file: File): String {
        val digest = MessageDigest.getInstance("SHA-256")
        file.inputStream().use { input ->
            val buffer = ByteArray(1 shl 20)
            while (true) {
                val read = input.read(buffer)
                if (read < 0) break
                digest.update(buffer, 0, read)
            }
        }
        val hex = StringBuilder(64)
        for (byte in digest.digest()) {
            val value = byte.toInt() and 0xFF
            hex.append(HEX_DIGITS[value ushr 4]).append(HEX_DIGITS[value and 0x0F])
        }
        return hex.toString()
    }

    private const val HEX_DIGITS = "0123456789abcdef"

    private fun assetKey(file: File): String? =
        if (file.isFile) "${file.absolutePath}|${file.length()}|${file.lastModified()}" else null

    private fun describeInstalledBuild(context: Context): JSONObject {
        val json = JSONObject()
        runCatching {
            val info = context.packageManager.getPackageInfo(context.packageName, 0)
            val application = context.applicationInfo
            val baseApk = File(application.sourceDir)
            val splits = application.splitSourceDirs
            json.put("applicationId", context.packageName)
            json.put("versionName", info.versionName ?: "")
            json.put("versionCode", PackageInfoCompat.getLongVersionCode(info))
            json.put("debuggable", (application.flags and ApplicationInfo.FLAG_DEBUGGABLE) != 0)
            json.put("installedFromSplits", splits != null && splits.isNotEmpty())
            json.put("apkBytes", baseApk.length())
            json.put("apkSha256", sha256(baseApk))
        }.onFailure { failure ->
            json.put("identityError", failure.message ?: failure.javaClass.simpleName)
        }
        json.put("deviceManufacturer", Build.MANUFACTURER ?: "")
        json.put("deviceModel", Build.MODEL ?: "")
        json.put("androidRelease", Build.VERSION.RELEASE ?: "")
        json.put("androidSdk", Build.VERSION.SDK_INT)
        json.put("abi", Build.SUPPORTED_ABIS.firstOrNull() ?: "")
        return json
    }
}
