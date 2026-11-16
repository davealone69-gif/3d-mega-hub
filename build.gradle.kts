buildscript {
    repositories {
        google()
        mavenCentral()
    }
    dependencies {
        // AGP 9 compiles Kotlin itself and carries KGP 2.2.10 by default. Raising the classpath
        // version to the catalog's `kotlin` is the documented way to keep targeting 2.4.21.
        classpath("org.jetbrains.kotlin:kotlin-gradle-plugin:2.4.21")
    }
}

plugins {
    alias(libs.plugins.android.application) apply false
    alias(libs.plugins.android.library) apply false
    alias(libs.plugins.kotlin.compose) apply false
}
