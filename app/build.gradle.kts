plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.kotlin.android)
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
    compileSdk = 35

    defaultConfig {
        applicationId = "com.davemega.hub"
        minSdk = 26
        targetSdk = 35
        versionCode = 1
        versionName = "0.1.0"
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
    kotlinOptions { jvmTarget = "17" }
    buildFeatures { compose = true }
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

    debugImplementation(libs.androidx.compose.ui.tooling)
    testImplementation(libs.junit)
}
