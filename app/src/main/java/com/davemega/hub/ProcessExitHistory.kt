package com.davemega.hub

import android.app.ActivityManager
import android.app.ApplicationExitInfo
import android.content.Context
import android.os.Build
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/**
 * What Android itself recorded about how earlier runs of this app ended.
 *
 * The numbers come from [ActivityManager.getHistoricalProcessExitReasons] (Android 11+). They are
 * the operating system's record, not something the app counts about itself, so a native crash that
 * killed the process before any app code could react is still reported here on the next launch.
 */
internal data class ProcessExitHistory(
    val available: Boolean,
    val examined: Int,
    val javaCrashes: Int,
    val nativeCrashes: Int,
    val anrs: Int,
    val lowMemoryKills: Int,
    val latestProblem: String?
) {
    val problemCount: Int get() = javaCrashes + nativeCrashes + anrs

    val headline: String
        get() = when {
            !available -> "Not available on this Android version"
            examined == 0 -> "No earlier exits recorded by Android"
            else -> "$javaCrashes crash(es) · $nativeCrashes native crash(es) · $anrs ANR(s) in the last $examined exits"
        }

    val detail: String
        get() = when {
            !available -> "Android 11 or newer is required to read the system's process exit history."
            latestProblem != null -> "Most recent problem: $latestProblem · low-memory kills: $lowMemoryKills"
            else -> "Recorded by Android, not by the app. Low-memory kills: $lowMemoryKills."
        }
}

internal fun readProcessExitHistory(context: Context, maxEntries: Int = 16): ProcessExitHistory {
    val unavailable = ProcessExitHistory(false, 0, 0, 0, 0, 0, null)
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) return unavailable
    val manager = context.getSystemService(Context.ACTIVITY_SERVICE) as? ActivityManager ?: return unavailable
    val exits: List<ApplicationExitInfo> = try {
        manager.getHistoricalProcessExitReasons(context.packageName, 0, maxEntries)
    } catch (failure: RuntimeException) {
        return unavailable
    }
    var javaCrashes = 0
    var nativeCrashes = 0
    var anrs = 0
    var lowMemoryKills = 0
    var latestProblem: String? = null
    val timeFormat = SimpleDateFormat("yyyy-MM-dd HH:mm", Locale.US)
    for (exit in exits) {
        val label = when (exit.reason) {
            ApplicationExitInfo.REASON_CRASH -> { javaCrashes++; "Java crash" }
            ApplicationExitInfo.REASON_CRASH_NATIVE -> { nativeCrashes++; "native crash" }
            ApplicationExitInfo.REASON_ANR -> { anrs++; "ANR" }
            ApplicationExitInfo.REASON_LOW_MEMORY -> { lowMemoryKills++; null }
            else -> null
        }
        if (label != null && latestProblem == null) {
            val description = exit.description?.take(120)?.let { " ($it)" }.orEmpty()
            latestProblem = "$label at ${timeFormat.format(Date(exit.timestamp))}$description"
        }
    }
    return ProcessExitHistory(true, exits.size, javaCrashes, nativeCrashes, anrs, lowMemoryKills, latestProblem)
}
