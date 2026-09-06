plugins {
    alias(libs.plugins.android.application)
}

android {
    namespace = "com.bessy.thebeast"
    compileSdk = 34

    ndkVersion = "27.3.13750724"

    defaultConfig {
        applicationId = "com.bessy.thebeast"
        minSdk = 24
        targetSdk = 34
        versionCode = 1
        versionName = "1.0"

        // Single 64-bit architecture to eliminate multi-binary bloat
        ndk {
            abiFilters.add("arm64-v8a")
        }

        // C compiler flags targeting minimal binary size
        externalNativeBuild {
            cmake {
                arguments(
                    "-DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake",
                    "-DVCPKG_CHAINLOAD_TOOLCHAIN_FILE=C:/Users/krish/AppData/Local/Android/Sdk/ndk/27.3.13750724/build/cmake/android.toolchain.cmake",
                    "-DVCPKG_TARGET_TRIPLET=arm64-android",
                    "-DANDROID_ABI=arm64-v8a",
                    "-DANDROID_PLATFORM=android-24"
                )
                cFlags("-Os", "-ffunction-sections", "-fdata-sections", "-fvisibility=hidden")
            }
        }
    }

    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = true
            isShrinkResources = true
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_11
        targetCompatibility = JavaVersion.VERSION_11
    }

    // Strip unused metadata and Gradle features
    buildFeatures {
        buildConfig = false
        resValues = false
        aidl = false
    }
}

dependencies {
}