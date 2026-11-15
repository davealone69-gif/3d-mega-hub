package com.davemega.hub

import android.content.ContentValues
import android.content.Context
import android.content.Intent
import android.graphics.Bitmap
import android.graphics.Canvas
import android.net.Uri
import android.os.Build
import android.os.Environment
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import android.provider.MediaStore
import android.util.Log
import android.view.PixelCopy
import android.view.SurfaceView
import android.view.View
import android.view.ViewGroup
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import androidx.test.runner.lifecycle.ActivityLifecycleMonitorRegistry
import androidx.test.runner.lifecycle.Stage
import androidx.test.uiautomator.By
import androidx.test.uiautomator.UiDevice
import androidx.test.uiautomator.UiObject2
import java.io.File
import java.io.FileOutputStream
import java.io.OutputStream
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicReference
import kotlin.math.abs
import org.json.JSONObject
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith

/**
 * A rigged file from elsewhere, with named clips, through the app: it must open in the glTF
 * viewer, list its clips by name, play the one that is tapped, and the picture must follow the
 * skeleton.
 *
 * The file is `rigged_test_figure.glb` (a figure of boxes, seven joints, clips "Wave", "March" and
 * "Bow"), written by `tools/glb_figures/make_test_figure.py single`. It belongs to the project.
 *
 * Until 2026-11-15 nothing in CI gave the glTF viewer a model with a skeleton or a clip: the
 * bundled sample has neither.
 */
@RunWith(AndroidJUnit4::class)
class GlbClipPreviewTest {
    @After
    fun closeTheAppAndForgetTheTestModel() {
        val instrumentation = InstrumentationRegistry.getInstrumentation()
        instrumentation.runOnMainSync {
            val monitor = ActivityLifecycleMonitorRegistry.getInstance()
            Stage.values().filter { it != Stage.DESTROYED }.forEach { stage ->
                monitor.getActivitiesInStage(stage).forEach { it.finish() }
            }
        }
        instrumentation.targetContext.getSharedPreferences("mega_hub", 0).edit()
            .remove("avatar_uri")
            .remove("avatar_name")
            .commit()
        SystemClock.sleep(1_500L)
    }

    @Test(timeout = 420_000)
    fun importedRiggedFileListsItsClipsByNameAndPlaysTheOneThatIsTapped() {
        val instrumentation = InstrumentationRegistry.getInstrumentation()
        val context = instrumentation.targetContext
        val device = UiDevice.getInstance(instrumentation)

        val directory = File(context.filesDir, "avatars")
        assertTrue("Could not create the app-private avatar test directory", directory.isDirectory || directory.mkdirs())
        val glb = File(directory, "rigged-test-figure.glb")
        instrumentation.context.assets.open("rigged_test_figure.glb").use { source ->
            FileOutputStream(glb).use { target -> source.copyTo(target) }
        }
        assertTrue("The rigged test figure is empty", glb.length() > 0L)

        val summary = GlbPreviewRouting.summarize(glb)
        assertNotNull("The rigged test figure is not a readable GLB", summary)
        assertEquals("clips in the rigged test figure", 3, summary!!.animationCount)
        assertEquals("skeletons in the rigged test figure", 1, summary.skinCount)
        assertTrue(
            "A file from elsewhere that carries clips must open in the glTF viewer: $summary",
            GlbPreviewRouting.opensInAnimationPreview(summary)
        )

        context.getSharedPreferences("mega_hub", 0).edit()
            .putString("avatar_uri", Uri.fromFile(glb).toString())
            .putString("avatar_name", glb.name)
            .commit()
        val launch = Intent(context, MainActivity::class.java)
            .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK)
            .putExtra(EXTRA_OPEN_AVATAR_STUDIO, true)
        val activity = instrumentation.startActivitySync(launch)
        assertNotNull("MainActivity did not launch", activity)

        val viewer = waitFor("the glTF viewer to be created for a file with clips", 45_000L, { "No FilamentPreviewView in the window." }) {
            onMain { findViewer(activity.window.decorView) }
        }
        fun state(): FilamentPreviewView.PlaybackState = onMain { viewer.playbackState() }

        /* ---- the file loads, and its clips are listed by the names in the file ---- */
        val loaded = waitFor("the rigged test figure to be shown by the glTF viewer", 150_000L, { "Last state: ${state()}" }) {
            state().takeIf { it.modelShown }
        }
        assertEquals("clip labels, in the file's order", listOf("Wave", "March", "Bow"), loaded.clipNames)
        assertEquals("the first clip is the one chosen when a file opens", 0, loaded.selectedClip)
        assertTrue("the first clip plays when a file opens: $loaded", loaded.playing)
        assertEquals("length of the clip Wave, in seconds", 2.0f, loaded.durationSeconds, 0.02f)

        /* ---- a tap on a clip's chip under the stage chooses that clip ---- */
        val width = device.displayWidth
        val height = device.displayHeight
        // The strip lies under the 440 dp stage, below the fold of a small screen. Move the page
        // up first: by a slow drag on the category rail beside the 3D view. A finger on the view
        // itself turns the model instead of moving the page, and a quick swipe would fling it.
        var chip: UiObject2? = null
        var drags = 0
        while ((chip == null || chip.visibleBounds.height() < 48) && drags < 6) {
            val x = (width * 0.84f).toInt()
            device.swipe(x, (height * 0.70f).toInt(), x, (height * 0.40f).toInt(), 200)
            SystemClock.sleep(700L)
            chip = device.findObject(By.text("March"))
            drags++
        }
        assertNotNull(
            "No chip named March under the stage after $drags drags. The clip strip is missing or off the screen. State: ${state()}",
            chip
        )
        chip!!.click()
        val tapped = waitFor("the tap on the March chip to choose the second clip", 20_000L, { "Last state: ${state()}" }) {
            state().takeIf { it.selectedClip == 1 }
        }
        assertTrue("the tapped clip plays: $tapped", tapped.playing)
        assertEquals("length of the clip March, in seconds", 1.0f, tapped.durationSeconds, 0.02f)

        /* ---- a playing clip moves on with the frames that are drawn ---- */
        val earlier = state()
        val later = waitFor("the playing clip to move on over three drawn frames", 60_000L, { "Earlier: $earlier. Last state: ${state()}" }) {
            state().takeIf {
                it.renderedFrames >= earlier.renderedFrames + 3L && it.positionSeconds != earlier.positionSeconds
            }
        }
        assertTrue("the clip position stays inside the clip: $later", later.positionSeconds in 0.0f..1.0f)

        /* ---- the picture follows the skeleton: hold Wave at two times and compare ---- */
        val surfaceView = onMain { viewer.getChildAt(0) } as? SurfaceView
            ?: throw AssertionError("The glTF viewer's first child is not its SurfaceView")
        onMain {
            viewer.selectAnimation(0)
            viewer.setAnimationPlaying(false)
        }
        val armDown = holdAndCapture(viewer, surfaceView, 0.0f)
        val armRaised = holdAndCapture(viewer, surfaceView, 1.0f)
        val armDownAgain = holdAndCapture(viewer, surfaceView, 0.0f)
        val held = state()
        assertEquals("Wave is the held clip", 0, held.selectedClip)
        assertTrue("the held clip is paused: $held", !held.playing)

        val sheet = Bitmap.createBitmap(armDown.width + armRaised.width, armDown.height, Bitmap.Config.ARGB_8888)
        Canvas(sheet).apply {
            drawBitmap(armDown, 0f, 0f, null)
            drawBitmap(armRaised, armDown.width.toFloat(), 0f, null)
        }
        val saved = runCatching {
            saveToDownloads(context, "gltf_viewer_clip_poses.png") { output ->
                check(sheet.compress(Bitmap.CompressFormat.PNG, 100, output)) { "Could not encode the two poses" }
            }
        }
        saved.exceptionOrNull()?.let { Log.w("AvatarForge", "Could not save gltf_viewer_clip_poses.png to Downloads", it) }

        val drawn = samplesUnlikeTheCorner(armDown)
        val moved = differingSamples(armDown, armRaised)
        val returned = differingSamples(armDown, armDownAgain)
        val evidence = JSONObject()
            .put("asset", glb.name)
            .put("clips", loaded.clipNames.joinToString(","))
            .put("tappedClip", tapped.clipNames.getOrNull(tapped.selectedClip) ?: "")
            .put("dragsToReachTheStrip", drags)
            .put("framesDrawn", held.renderedFrames)
            .put("surface", "${armDown.width}x${armDown.height}")
            .put("gridSamples", GRID * GRID)
            .put("samplesUnlikeTheBackground", drawn)
            .put("samplesChangedBetweenTwoTimesOfWave", moved)
            .put("samplesChangedAfterReturningToTheFirstTime", returned)
            .put("screenshotSaved", saved.isSuccess)
        Log.i("AvatarForge", "CLIP_PREVIEW_EVIDENCE $evidence")

        assertTrue("The glTF viewer drew no figure: $evidence", drawn >= GRID * GRID / 50)
        assertTrue(
            "Holding the clip Wave at 0 s and at 1 s gave the same picture: the skeleton does not move the mesh. $evidence",
            moved >= GRID * GRID / 100
        )
        assertTrue(
            "Going back to 0 s did not give the first picture back: $evidence",
            returned * 4 <= moved
        )

        /* ---- evidence only, nothing below can fail the test: the screen as a person sees it,
           with the clip strip and the Motion section of this file ---- */
        var motionLine = ""
        var bowChosen = false
        val screenSaved = runCatching {
            device.findObject(By.text("Bow"))?.click()
            SystemClock.sleep(1_500L)
            bowChosen = state().selectedClip == 2
            device.findObject(By.text("Motion"))?.click()
            SystemClock.sleep(1_000L)
            val x = (width * 0.84f).toInt()
            device.swipe(x, (height * 0.55f).toInt(), x, (height * 0.25f).toInt(), 200)
            SystemClock.sleep(1_500L)
            motionLine = device.findObject(By.textStartsWith("Animations in this file"))?.text ?: ""
            val screenFile = File(context.cacheDir, "gltf_viewer_clip_strip_screen.png")
            check(device.takeScreenshot(screenFile)) { "UiDevice could not take a screenshot" }
            saveToDownloads(context, screenFile.name) { output ->
                screenFile.inputStream().use { input -> input.copyTo(output) }
            }
        }
        screenSaved.exceptionOrNull()?.let { Log.w("AvatarForge", "Could not save the screen with the clip strip", it) }
        val screenEvidence = JSONObject()
            .put("tapOnBowChoseTheThirdClip", bowChosen)
            .put("motionLine", motionLine)
            .put("screenshotSaved", screenSaved.isSuccess)
        Log.i("AvatarForge", "CLIP_SCREEN_EVIDENCE $screenEvidence")
    }

    private fun findViewer(view: View): FilamentPreviewView? {
        if (view is FilamentPreviewView) return view
        if (view is ViewGroup) {
            for (index in 0 until view.childCount) {
                findViewer(view.getChildAt(index))?.let { return it }
            }
        }
        return null
    }

    /** Runs [block] on the main thread, where the viewer's state lives, and returns its result. */
    private fun <T> onMain(block: () -> T): T {
        val result = AtomicReference<T>()
        InstrumentationRegistry.getInstrumentation().runOnMainSync { result.set(block()) }
        return result.get()
    }

    private fun <T : Any> waitFor(what: String, timeoutMs: Long, describe: () -> String, probe: () -> T?): T {
        val deadline = SystemClock.elapsedRealtime() + timeoutMs
        while (true) {
            probe()?.let { return it }
            if (SystemClock.elapsedRealtime() >= deadline) {
                throw AssertionError("Timed out after $timeoutMs ms waiting for $what. ${describe()}")
            }
            SystemClock.sleep(250L)
        }
    }

    /** Holds the chosen clip at [seconds], waits for three frames drawn with it, and copies the surface. */
    private fun holdAndCapture(viewer: FilamentPreviewView, surfaceView: SurfaceView, seconds: Float): Bitmap {
        val before = onMain {
            viewer.seekAnimation(seconds)
            viewer.playbackState().renderedFrames
        }
        waitFor("three frames drawn with the clip held at $seconds s", 60_000L, { "Last state: ${onMain { viewer.playbackState() }}" }) {
            onMain { viewer.playbackState() }.takeIf { it.renderedFrames >= before + 3L }
        }
        // A drawn frame reaches the compositor a little later than the renderer reports it.
        SystemClock.sleep(500L)
        return capture(surfaceView)
    }

    /** PixelCopy of the viewer's own surface, retrying only the documented transient results. */
    private fun capture(surfaceView: SurfaceView): Bitmap {
        val instrumentation = InstrumentationRegistry.getInstrumentation()
        var lastResult = PixelCopy.ERROR_UNKNOWN
        for (delayMs in longArrayOf(0L, 250L, 500L, 1_000L, 2_000L)) {
            if (delayMs > 0L) SystemClock.sleep(delayMs)
            val candidate = Bitmap.createBitmap(surfaceView.width, surfaceView.height, Bitmap.Config.ARGB_8888)
            val latch = CountDownLatch(1)
            var result = PixelCopy.ERROR_UNKNOWN
            instrumentation.runOnMainSync {
                PixelCopy.request(surfaceView, candidate, { copyResult ->
                    result = copyResult
                    latch.countDown()
                }, Handler(Looper.getMainLooper()))
            }
            if (latch.await(5, TimeUnit.SECONDS)) {
                lastResult = result
                if (result == PixelCopy.SUCCESS) return candidate
            }
            candidate.recycle()
        }
        throw AssertionError("PixelCopy could not capture the glTF viewer's surface; lastResult=$lastResult")
    }

    /** Of a GRID x GRID lattice of samples, how many differ by more than 24 in some colour channel. */
    private fun differingSamples(first: Bitmap, second: Bitmap): Int {
        var differing = 0
        for (gy in 0 until GRID) {
            val firstY = ((gy + 0.5f) * first.height / GRID).toInt().coerceIn(0, first.height - 1)
            val secondY = ((gy + 0.5f) * second.height / GRID).toInt().coerceIn(0, second.height - 1)
            for (gx in 0 until GRID) {
                val firstX = ((gx + 0.5f) * first.width / GRID).toInt().coerceIn(0, first.width - 1)
                val secondX = ((gx + 0.5f) * second.width / GRID).toInt().coerceIn(0, second.width - 1)
                if (channelDistance(first.getPixel(firstX, firstY), second.getPixel(secondX, secondY)) > 24) differing++
            }
        }
        return differing
    }

    /** How many lattice samples differ from the top-left corner, which the framed figure never covers. */
    private fun samplesUnlikeTheCorner(image: Bitmap): Int {
        val corner = image.getPixel(2, 2)
        var unlike = 0
        for (gy in 0 until GRID) {
            val y = ((gy + 0.5f) * image.height / GRID).toInt().coerceIn(0, image.height - 1)
            for (gx in 0 until GRID) {
                val x = ((gx + 0.5f) * image.width / GRID).toInt().coerceIn(0, image.width - 1)
                if (channelDistance(image.getPixel(x, y), corner) > 32) unlike++
            }
        }
        return unlike
    }

    private fun channelDistance(first: Int, second: Int): Int = maxOf(
        abs(((first shr 16) and 0xff) - ((second shr 16) and 0xff)),
        abs(((first shr 8) and 0xff) - ((second shr 8) and 0xff)),
        abs((first and 0xff) - (second and 0xff))
    )

    /** Evidence only: a copy in the public Downloads folder outlives the uninstall after the run. */
    private fun saveToDownloads(context: Context, displayName: String, write: (OutputStream) -> Unit) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) {
            throw IllegalStateException("MediaStore Downloads needs Android 10")
        }
        val resolver = context.contentResolver
        val values = ContentValues().apply {
            put(MediaStore.MediaColumns.DISPLAY_NAME, displayName)
            put(MediaStore.MediaColumns.MIME_TYPE, "image/png")
            put(MediaStore.MediaColumns.RELATIVE_PATH, "${Environment.DIRECTORY_DOWNLOADS}/3D Mega Hub Diagnostics/")
        }
        val uri = resolver.insert(MediaStore.Downloads.EXTERNAL_CONTENT_URI, values)
            ?: error("Android could not create a Downloads entry for $displayName")
        val stream = resolver.openOutputStream(uri) ?: error("Android could not open the Downloads entry for $displayName")
        stream.use(write)
    }

    private companion object {
        const val GRID = 96
    }
}
