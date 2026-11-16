/*
 * A stand-in for the part of JUnit 4 the unit tests of this repository use, for kotlin_host.sh.
 * It is NOT JUnit: CI runs the tests with the real one. It exists because no JUnit jar can be
 * fetched in the sandboxes this repository is worked on in.
 */
@file:Suppress("unused")

package org.junit

import kotlin.reflect.KClass

class NoneExpected : Throwable()

@Retention(AnnotationRetention.RUNTIME)
@Target(AnnotationTarget.FUNCTION)
annotation class Test(val timeout: Long = 0L, val expected: KClass<out Throwable> = NoneExpected::class)

object Assert {
    private fun text(message: String?, detail: String) = if (message.isNullOrEmpty()) detail else "$message $detail"

    @JvmStatic fun fail(message: String?): Nothing = throw AssertionError(message ?: "failed")
    @JvmStatic fun assertTrue(condition: Boolean) { if (!condition) fail("expected true") }
    @JvmStatic fun assertTrue(message: String?, condition: Boolean) { if (!condition) fail(message) }
    @JvmStatic fun assertFalse(condition: Boolean) { if (condition) fail("expected false") }
    @JvmStatic fun assertFalse(message: String?, condition: Boolean) { if (condition) fail(message) }
    @JvmStatic fun assertNull(value: Any?) { if (value != null) fail("expected null but was <$value>") }
    @JvmStatic fun assertNull(message: String?, value: Any?) { if (value != null) fail(text(message, "expected null but was <$value>")) }
    @JvmStatic fun assertNotNull(value: Any?) { if (value == null) fail("expected a value, was null") }
    @JvmStatic fun assertNotNull(message: String?, value: Any?) { if (value == null) fail(text(message, "expected a value, was null")) }
    @JvmStatic fun assertSame(expected: Any?, actual: Any?) { if (expected !== actual) fail("expected same:<$expected> was not:<$actual>") }
    @JvmStatic fun assertEquals(expected: Any?, actual: Any?) { if (expected != actual) fail("expected:<$expected> but was:<$actual>") }
    @JvmStatic fun assertEquals(message: String?, expected: Any?, actual: Any?) {
        if (expected != actual) fail(text(message, "expected:<$expected> but was:<$actual>"))
    }
    @JvmStatic fun assertEquals(expected: Float, actual: Float, delta: Float) {
        if (!(Math.abs(expected - actual) <= delta)) fail("expected:<$expected> but was:<$actual>")
    }
    @JvmStatic fun assertEquals(message: String?, expected: Float, actual: Float, delta: Float) {
        if (!(Math.abs(expected - actual) <= delta)) fail(text(message, "expected:<$expected> but was:<$actual>"))
    }
    @JvmStatic fun assertEquals(expected: Double, actual: Double, delta: Double) {
        if (!(Math.abs(expected - actual) <= delta)) fail("expected:<$expected> but was:<$actual>")
    }
    @JvmStatic fun assertEquals(message: String?, expected: Double, actual: Double, delta: Double) {
        if (!(Math.abs(expected - actual) <= delta)) fail(text(message, "expected:<$expected> but was:<$actual>"))
    }
    @JvmStatic fun assertArrayEquals(expected: FloatArray, actual: FloatArray, delta: Float) {
        if (expected.size != actual.size) fail("array lengths differ: ${expected.size} and ${actual.size}")
        for (i in expected.indices) if (!(Math.abs(expected[i] - actual[i]) <= delta)) fail("arrays first differ at [$i]: ${expected[i]} and ${actual[i]}")
    }
    @JvmStatic fun assertArrayEquals(expected: IntArray, actual: IntArray) {
        if (!expected.contentEquals(actual)) fail("expected:<${expected.toList()}> but was:<${actual.toList()}>")
    }
    @JvmStatic fun assertArrayEquals(expected: ByteArray, actual: ByteArray) {
        if (!expected.contentEquals(actual)) fail("byte arrays differ")
    }
    @JvmStatic fun assertArrayEquals(expected: Array<out Any?>, actual: Array<out Any?>) {
        if (!expected.contentDeepEquals(actual)) fail("expected:<${expected.toList()}> but was:<${actual.toList()}>")
    }
    @JvmStatic fun <T : Throwable> assertThrows(expected: Class<T>, body: () -> Unit): T {
        try {
            body()
        } catch (thrown: Throwable) {
            if (expected.isInstance(thrown)) return expected.cast(thrown)!!
            throw AssertionError("expected ${expected.name} but ${thrown.javaClass.name} was thrown: ${thrown.message}", thrown)
        }
        throw AssertionError("expected ${expected.name} to be thrown, but nothing was")
    }
    @JvmStatic fun <T : Throwable> assertThrows(message: String?, expected: Class<T>, body: () -> Unit): T =
        try { assertThrows(expected, body) } catch (failure: AssertionError) { throw AssertionError(text(message, failure.message ?: ""), failure) }
}
