plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

extra["kairoProduct"] = "kairo64"
extra["kairoLabel"] = "Kairo64"
apply(from = rootProject.file("shared/gradle/kairo-app.gradle"))

// The MEGA65 Open ROMs, which the app ships (third_party/open-roms/bin), in assets/openroms;
// MainActivity copies them out before the first game.
val openRomsAssets = layout.buildDirectory.dir("generated/openroms-assets")
val stageOpenRoms = tasks.register<Copy>("stageOpenRoms") {
    from(rootProject.file("third_party/open-roms/bin")) {
        include("kernal_generic.rom", "basic_generic.rom", "chargen_openroms.rom")
    }
    into(openRomsAssets.map { it.dir("openroms") })
}
android.sourceSets.getByName("main").assets.srcDir(openRomsAssets)
tasks.named("preBuild") { dependsOn(stageOpenRoms) }

dependencies {
    testImplementation("junit:junit:4.13.2")
}
