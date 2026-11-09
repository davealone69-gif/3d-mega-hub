package com.davemega.hub.avatarforge

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class NativeCancellationTokenTest {
    @Test
    fun observesOwnerInterruptionFromAnotherThread() {
        val owner = Thread.currentThread()
        val token = NativeCancellationToken(owner)
        val before = booleanArrayOf(true)
        val after = booleanArrayOf(false)

        val worker = Thread {
            before[0] = token.shouldCancel()
            while (!owner.isInterrupted) Thread.yield()
            after[0] = token.shouldCancel()
        }
        worker.start()
        worker.join(2_000)

        assertFalse("owner should not be interrupted before cancellation", before[0])
        // Re-run with a controlled worker so interruption is guaranteed to be observed.
        val observingWorker = Thread {
            while (!owner.isInterrupted) Thread.yield()
            after[0] = token.shouldCancel()
        }
        observingWorker.start()
        owner.interrupt()
        observingWorker.join(2_000)
        assertTrue("worker must observe the generation owner's interrupt", after[0])
        Thread.interrupted() // Restore the test thread's interrupt status.
    }
}
