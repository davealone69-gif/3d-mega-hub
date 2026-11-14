package com.davemega.hub.avatarforge

import java.util.concurrent.atomic.AtomicBoolean
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class NativeCancellationTokenTest {
    @Test
    fun observesOwnerInterruptionFromAnotherThread() {
        val owner = Thread.currentThread()
        val token = NativeCancellationToken(owner)
        val before = AtomicBoolean(true)
        val beforeWorker = Thread { before.set(token.shouldCancel()) }
        beforeWorker.start()
        beforeWorker.join()
        assertFalse("owner should not be interrupted before cancellation", before.get())

        owner.interrupt()
        try {
            val after = AtomicBoolean(false)
            val finished = AtomicBoolean(false)
            Thread {
                after.set(token.shouldCancel())
                finished.set(true)
            }.start()
            // Do not call join/await while this test thread is intentionally interrupted.
            while (!finished.get()) Thread.yield()
            assertTrue("worker must observe the generation owner's interrupt", after.get())
        } finally {
            Thread.interrupted() // Restore the test thread's interrupt status.
        }
    }
}
