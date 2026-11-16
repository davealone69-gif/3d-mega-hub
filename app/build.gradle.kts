plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.kotlin.compose)
}

val releaseStoreFilePath = providers.gradleProperty("releaseStoreFile")
    .orElse(providers.environmentVariable("ANDROID_RELEASE_STORE_FILE"))
    .orNull
val releaseStorePassword = providers.gradleProperty("releaseStorePassword")
    .orElse(providers.environmentVariable("ANDROID_RELEASE_STORE_PASSWORD"))
    .orNull
val releaseKeyAlias = providers.gradleProperty("releaseKeyAlias")
    .orElse(providers.environmentVariable("ANDROID_RELEASE_KEY_ALIAS"))
    .orNull
val releaseKeyPassword = providers.gradleProperty("releaseKeyPassword")
    .orElse(providers.environmentVariable("ANDROID_RELEASE_KEY_PASSWORD"))
    .orNull
val releaseStoreFile = releaseStoreFilePath?.let { rootProject.file(it) }
val releaseSigningPreflight = tasks.register("validateReleaseSigning") {
    group = "verification"
    description = "Fail release packaging if a readable keystore and credentials are unavailable."
    doLast {
        val missing = buildList {
            if (releaseStoreFilePath.isNullOrBlank()) add("releaseStoreFile / ANDROID_RELEASE_STORE_FILE")
            if (releaseStorePassword.isNullOrBlank()) add("releaseStorePassword / ANDROID_RELEASE_STORE_PASSWORD")
            if (releaseKeyAlias.isNullOrBlank()) add("releaseKeyAlias / ANDROID_RELEASE_KEY_ALIAS")
            if (releaseKeyPassword.isNullOrBlank()) add("releaseKeyPassword / ANDROID_RELEASE_KEY_PASSWORD")
            if (releaseStoreFile != null && (!releaseStoreFile.isFile || !releaseStoreFile.canRead())) {
                add("a readable release keystore file")
            }
        }
        if (missing.isNotEmpty()) {
            throw GradleException(
                "BLOCKED: signed release packaging requires valid signing credentials; missing or invalid: " +
                    missing.joinToString() + ". No unsigned release APK/AAB will be produced."
            )
        }
    }
}

android {
    namespace = "com.davemega.hub"
    // Compose 1.12.1 (BOM 2026.09.00) publishes AAR metadata requiring compilation against API 37.
    // targetSdk stays 36 deliberately: raising it changes runtime behaviour, which no device here
    // has verified.
    compileSdk = 37

    defaultConfig {
        applicationId = "com.davemega.hub"
        minSdk = 26
        targetSdk = 36
        versionCode = 2
        versionName = "0.2.0"
        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
    }

    signingConfigs {
        create("release") {
            storeFile = releaseStoreFile
            storePassword = releaseStorePassword
            keyAlias = releaseKeyAlias
            keyPassword = releaseKeyPassword
        }
    }

    buildTypes {
        debug { isMinifyEnabled = false }
        release {
            isMinifyEnabled = true
            isShrinkResources = true
            signingConfig = signingConfigs.getByName("release")
            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    buildFeatures { compose = true }
}

// AGP 9 removed the `kotlinOptions` DSL; Kotlin 2.4 configures the JVM target through the Kotlin
// Gradle plugin's compilerOptions instead.
kotlin {
    compilerOptions {
        jvmTarget.set(org.jetbrains.kotlin.gradle.dsl.JvmTarget.JVM_17)
    }
}

tasks.configureEach {
    if (name == "packageRelease" || name == "packageReleaseBundle" || name == "signReleaseBundle") {
        dependsOn(releaseSigningPreflight)
    }
}

dependencies {
    implementation(libs.androidx.core.ktx)
    implementation(libs.androidx.activity.compose)
    implementation(platform(libs.androidx.compose.bom))
    implementation(libs.androidx.compose.ui)
    implementation(libs.androidx.compose.ui.graphics)
    implementation(libs.androidx.compose.ui.tooling.preview)
    implementation(libs.androidx.compose.material3)

    implementation(libs.filament.android)
    implementation(libs.filament.utils.android)
    implementation(libs.filament.gltfio.android)
    implementation(project(":avatarforge-engine"))

    debugImplementation(libs.androidx.compose.ui.tooling)
    testImplementation(libs.junit)
    testImplementation("org.json:json:20240303")
    androidTestImplementation(libs.androidx.test.runner)
    androidTestImplementation(libs.androidx.test.core)
    androidTestImplementation(libs.androidx.test.ext.junit)
    androidTestImplementation(libs.androidx.test.uiautomator)
}
