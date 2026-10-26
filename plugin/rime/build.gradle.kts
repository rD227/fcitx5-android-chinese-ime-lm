plugins {
    id("org.fcitx.fcitx5.android.app-convention")
    id("org.fcitx.fcitx5.android.plugin-app-convention")
    id("org.fcitx.fcitx5.android.native-app-convention")
    id("org.fcitx.fcitx5.android.build-metadata")
    id("org.fcitx.fcitx5.android.data-descriptor")
    id("org.fcitx.fcitx5.android.fcitx-component")
}

val rimeNeural = providers.gradleProperty("rimeNeural").map(String::toBoolean).getOrElse(false)

android {
    namespace = "org.fcitx.fcitx5.android.plugin.rime"

    defaultConfig {
        applicationId = "org.fcitx.fcitx5.android.plugin.rime"
        if (rimeNeural) {
            applicationId = "org.fcitx.fcitx5.android.plugin.rime.neural"
        }

        @Suppress("UnstableApiUsage")
        externalNativeBuild {
            cmake {
                arguments("-DRIME_NEURAL=${if (rimeNeural) "ON" else "OFF"}")
                targets(
                    "rime"
                )
            }
        }
    }

    buildFeatures {
        resValues = true
    }

    buildTypes {
        release {
            resValue("string", "app_name", if (rimeNeural) "Fcitx5 (Rime · Chinese IME LM)" else "@string/app_name_release")
            proguardFile("proguard-rules.pro")
        }
        debug {
            resValue("string", "app_name", if (rimeNeural) "Fcitx5 (Rime · Chinese IME LM)" else "@string/app_name_debug")
        }
    }

    if (rimeNeural) {
        sourceSets.getByName("release").res.srcDir("neural/res")
        sourceSets.getByName("debug").res.srcDir("neural/res")
    }

    packaging {
        jniLibs {
            excludes += setOf(
                "**/libc++_shared.so",
                "**/libFcitx5*"
            )
        }
    }
}

fcitxComponent {
    installPrebuiltAssets = true
}

generateDataDescriptor {
    symlinks.put("usr/share/rime-data/opencc", "usr/share/opencc")
}

dependencies {
    implementation(project(":lib:fcitx5"))
    implementation(project(":lib:plugin-base"))
}
