package com.davemega.hub.avatarforge

import android.content.ContentValues
import android.content.Context
import android.net.Uri
import android.os.Build
import android.os.Environment
import android.provider.MediaStore
import java.io.File
import java.io.OutputStream

/**
 * Persists diagnostic artifacts in the user's public Downloads collection on Android 10+.
 * On older Android versions, uses the app-specific Downloads directory rather than requesting
 * broad shared-storage access.
 */
internal object DiagnosticDownloads {
    private const val DIRECTORY_NAME = "3D Mega Hub Diagnostics"

    fun save(
        context: Context,
        displayName: String,
        mimeType: String,
        write: (OutputStream) -> Unit
    ): Uri {
        require(displayName.isNotBlank() && File(displayName).name == displayName) {
            "Diagnostic filename must not contain a path."
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            val resolver = context.contentResolver
            val collection = MediaStore.Downloads.EXTERNAL_CONTENT_URI
            val relativePath = "${Environment.DIRECTORY_DOWNLOADS}/$DIRECTORY_NAME/"
            val existing = resolver.query(
                collection,
                arrayOf(MediaStore.MediaColumns._ID),
                "${MediaStore.MediaColumns.DISPLAY_NAME}=? AND ${MediaStore.MediaColumns.RELATIVE_PATH}=?",
                arrayOf(displayName, relativePath),
                null
            )?.use { cursor ->
                if (cursor.moveToFirst()) {
                    Uri.withAppendedPath(collection, cursor.getLong(0).toString())
                } else null
            }
            val values = ContentValues().apply {
                put(MediaStore.MediaColumns.DISPLAY_NAME, displayName)
                put(MediaStore.MediaColumns.MIME_TYPE, mimeType)
                put(MediaStore.MediaColumns.RELATIVE_PATH, relativePath)
                put(MediaStore.MediaColumns.IS_PENDING, 1)
            }
            val uri = existing ?: resolver.insert(collection, values)
                ?: error("Android could not create a Downloads entry for $displayName.")
            if (existing != null) {
                check(resolver.update(uri, values, null, null) == 1) {
                    "Android could not prepare the existing Downloads entry for $displayName."
                }
            }
            try {
                resolver.openOutputStream(uri, "wt")?.use(write)
                    ?: error("Android could not open the Downloads entry for $displayName.")
                val completed = ContentValues().apply {
                    put(MediaStore.MediaColumns.IS_PENDING, 0)
                }
                check(resolver.update(uri, completed, null, null) == 1) {
                    "Android could not publish $displayName in Downloads."
                }
                return uri
            } catch (failure: Throwable) {
                if (existing == null) runCatching { resolver.delete(uri, null, null) }
                throw failure
            }
        }

        val directory = File(
            context.getExternalFilesDir(Environment.DIRECTORY_DOWNLOADS)
                ?: context.filesDir,
            DIRECTORY_NAME
        )
        check(directory.isDirectory || directory.mkdirs()) {
            "Could not create the app-specific diagnostic Downloads directory."
        }
        val destination = File(directory, displayName)
        val temporary = File(directory, "$displayName.part")
        try {
            temporary.outputStream().buffered().use(write)
            check(temporary.renameTo(destination)) {
                "Could not atomically publish $displayName in app-specific Downloads."
            }
        } catch (failure: Throwable) {
            temporary.delete()
            throw failure
        }
        return Uri.fromFile(destination)
    }

    fun locationLabel(): String =
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            "Downloads/$DIRECTORY_NAME"
        } else {
            "app-specific Downloads/$DIRECTORY_NAME"
        }
}
