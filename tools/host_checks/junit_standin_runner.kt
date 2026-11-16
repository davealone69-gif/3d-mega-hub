/* The runner of junit_standin.kt: runs every @Test method of the classes named on the command line. */
@file:JvmName("Runner")

package hostjunit

import java.lang.reflect.InvocationTargetException
import kotlin.system.exitProcess

fun main(arguments: Array<String>) {
    var run = 0
    var skipped = 0
    val failures = ArrayList<String>()
    for (name in arguments) {
        val type = Class.forName(name)
        for (method in type.declaredMethods.sortedBy { it.name }) {
            val test = method.getAnnotation(org.junit.Test::class.java) ?: continue
            val expected = test.expected.java
            run++
            try {
                method.invoke(type.getDeclaredConstructor().newInstance())
                if (expected != org.junit.NoneExpected::class.java) failures += "$name.${method.name}: expected ${expected.name}, nothing was thrown"
            } catch (wrapped: InvocationTargetException) {
                val cause = wrapped.targetException
                if (expected.isInstance(cause)) continue
                /* android.jar holds stubs: such a test needs the device or the Gradle build. */
                var stub: Throwable? = cause
                var needsAndroid = false
                while (stub != null) {
                    if (stub.message?.contains("Stub!") == true) needsAndroid = true
                    stub = stub.cause
                }
                if (needsAndroid) {
                    skipped++
                    run--
                    println("NEEDS ANDROID (not run here)  $name.${method.name}")
                } else {
                    failures += "$name.${method.name}: $cause\n" + cause.stackTrace.take(6).joinToString("\n") { "        at $it" }
                }
            }
        }
    }
    for (failure in failures) println("FAILED  $failure")
    println("host JUnit stand-in: $run test(s) run, ${failures.size} failed, $skipped need Android and were not run")
    exitProcess(if (failures.isEmpty()) 0 else 1)
}
