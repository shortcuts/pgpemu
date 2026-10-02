plugins {
    alias(libs.plugins.kotlin.jvm)
}

kotlin {
    jvmToolchain(17)
}

dependencies {
    compileOnly(libs.lintcheck.api)

    testImplementation(libs.lintcheck.core)
    testImplementation(libs.lintcheck.tests)
    testImplementation(libs.lintcheck.api)
    testImplementation(libs.junit)
}

tasks.jar {
    manifest {
        attributes("Lint-Registry-v2" to "com.pgpemu.companion.lint.checks.PgpIssueRegistry")
    }
}

// Lint's Kotlin/PSI analysis (used by lint-tests' TestLintTask) needs these
// module opens on JDK 17+ or UAST silently returns no results instead of throwing.
tasks.withType<Test>().configureEach {
    jvmArgs(
        "--add-opens=java.base/java.io=ALL-UNNAMED",
        "--add-opens=java.base/java.lang=ALL-UNNAMED",
        "--add-opens=java.base/java.lang.ref=ALL-UNNAMED",
        "--add-opens=java.base/java.lang.reflect=ALL-UNNAMED",
        "--add-opens=java.base/java.net=ALL-UNNAMED",
        "--add-opens=java.base/java.nio=ALL-UNNAMED",
        "--add-opens=java.base/java.nio.charset=ALL-UNNAMED",
        "--add-opens=java.base/java.text=ALL-UNNAMED",
        "--add-opens=java.base/java.util=ALL-UNNAMED",
        "--add-opens=java.base/java.util.concurrent=ALL-UNNAMED",
        "--add-opens=java.base/java.util.concurrent.atomic=ALL-UNNAMED",
        "--add-opens=java.base/java.security=ALL-UNNAMED",
        "--add-opens=java.base/jdk.internal.vm=ALL-UNNAMED",
        "--add-opens=java.base/sun.nio.ch=ALL-UNNAMED",
        "--add-opens=java.base/sun.nio.fs=ALL-UNNAMED",
        "--add-opens=java.base/sun.security.ssl=ALL-UNNAMED",
        "--add-opens=java.base/sun.security.util=ALL-UNNAMED",
        "--add-opens=java.desktop/java.awt.font=ALL-UNNAMED",
        "--add-opens=jdk.compiler/com.sun.tools.javac.file=ALL-UNNAMED",
        "--add-opens=jdk.compiler/com.sun.tools.javac.parser=ALL-UNNAMED",
        "--add-opens=jdk.compiler/com.sun.tools.javac.tree=ALL-UNNAMED",
        "--add-opens=jdk.compiler/com.sun.tools.javac.util=ALL-UNNAMED",
    )
}
