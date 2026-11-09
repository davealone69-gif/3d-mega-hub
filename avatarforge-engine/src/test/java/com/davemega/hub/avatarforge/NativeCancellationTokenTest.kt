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
        val beforeWorker = Thread { before[0] = token.shouldCancel() }
        beforeWorker.start()
        beforeWorker.join()
        assertFalse("owner should not be interrupted before cancellation", before[0])

        owner.interrupt()
        try {
            val after = booleanArrayOf(false)
            val afterWorker = Thread { after[0] = token.shouldCancel() }
            afterWorker.start()
            afterWorker.join()
            assertTrue("worker must observe the generation owner's interrupt", after[0])
        } finally {
            Thread.interrupted() // Restore the test thread's interrupt status.
        }
    }
}
