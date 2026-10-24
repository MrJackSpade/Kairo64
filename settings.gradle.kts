apply(from = "shared/gradle/android-settings.gradle")

rootProject.name = "Kairo64"
include(":kairo64", ":frontend")
project(":frontend").projectDir = file("shared/frontend")
