package com.pgpemu.companion.lint.checks

import com.android.tools.lint.checks.infrastructure.LintDetectorTest
import com.android.tools.lint.checks.infrastructure.TestFile
import com.android.tools.lint.checks.infrastructure.TestFiles.java
import com.android.tools.lint.checks.infrastructure.TestMode
import com.android.tools.lint.detector.api.Detector
import com.android.tools.lint.detector.api.Issue

/**
 * The detector's own logic (method resolution, argument mapping, `@Preview`
 * exemption) is language-agnostic UAST over resolved `PsiMethod`s, so these
 * fixtures use Java stand-ins for the Compose/annotation types instead of
 * real Kotlin sources — this environment's lint test harness (`TestLintTask`)
 * cannot parse Kotlin fixtures reliably, while the detector itself is
 * separately verified end-to-end against real Kotlin Compose call sites via
 * `./gradlew :core:designsystem:lintRelease`. Branch-walking for Kotlin-only
 * constructs (elvis `?:`, `when`, and the `contentDescription = ` semantics
 * assignment) is exercised only via that same manual Gradle verification,
 * not a `.kt` fixture, for the same reason.
 */
class HardcodedComposeStringDetectorTest : LintDetectorTest() {
    override fun getDetector(): Detector = HardcodedComposeStringDetector()

    override fun getIssues(): List<Issue> = listOf(HardcodedComposeStringDetector.ISSUE)

    private val composableStub: TestFile =
        java(
            """
            package androidx.compose.runtime;
            public @interface Composable {}
            """,
        ).indented()

    private val previewStub: TestFile =
        java(
            """
            package androidx.compose.ui.tooling.preview;
            public @interface Preview {}
            """,
        ).indented()

    private val material3Stub: TestFile =
        java(
            """
            package androidx.compose.material3;
            public class Material3 {
                public static void Text(String text) {}
                public static String stringResource(int id) { return ""; }
            }
            """,
        ).indented()

    private val customComponentStub: TestFile =
        java(
            """
            package com.example.custom;
            import androidx.compose.runtime.Composable;
            public class CustomComponent {
                @Composable
                public static void Render(String title) {}
                public static void NotComposable(String title) {}
            }
            """,
        ).indented()

    private val foundationStub: TestFile =
        java(
            """
            package androidx.compose.foundation;
            public class Foundation {
                public static void clickable(String onClickLabel) {}
            }
            """,
        ).indented()

    private val stubs = listOf(composableStub, previewStub, material3Stub, customComponentStub, foundationStub)

    fun testHardcodedStringInTextIsFlagged() {
        lint()
            .files(
                *stubs.toTypedArray(),
                java(
                    """
                    package com.example;
                    import androidx.compose.material3.Material3;
                    import static androidx.compose.material3.Material3.Text;
                    import androidx.compose.runtime.Composable;

                    public class Sample {
                        @Composable
                        public void sample() {
                            Text("Hardcoded");
                        }
                    }
                    """,
                ).indented(),
            ).issues(HardcodedComposeStringDetector.ISSUE)
            .run()
            .expectWarningCount(1)
    }

    fun testStringResourceIsClean() {
        lint()
            .files(
                *stubs.toTypedArray(),
                java(
                    """
                    package com.example;
                    import static androidx.compose.material3.Material3.Text;
                    import static androidx.compose.material3.Material3.stringResource;
                    import androidx.compose.runtime.Composable;

                    public class Sample {
                        @Composable
                        public void sample() {
                            Text(stringResource(1));
                        }
                    }
                    """,
                ).indented(),
            ).issues(HardcodedComposeStringDetector.ISSUE)
            .run()
            .expectClean()
    }

    fun testHardcodedStringInsidePreviewComposableIsClean() {
        lint()
            .files(
                *stubs.toTypedArray(),
                java(
                    """
                    package com.example;
                    import static androidx.compose.material3.Material3.Text;
                    import androidx.compose.runtime.Composable;
                    import androidx.compose.ui.tooling.preview.Preview;

                    public class Sample {
                        @Preview
                        @Composable
                        public void samplePreview() {
                            Text("Hardcoded preview text");
                        }
                    }
                    """,
                ).indented(),
            ).issues(HardcodedComposeStringDetector.ISSUE)
            .run()
            .expectClean()
    }

    fun testTernaryBranchesBothHardcodedAreFlaggedIndividually() {
        lint()
            .files(
                *stubs.toTypedArray(),
                java(
                    """
                    package com.example;
                    import static androidx.compose.material3.Material3.Text;
                    import androidx.compose.runtime.Composable;

                    public class Sample {
                        @Composable
                        public void sample(boolean cond) {
                            Text(cond ? "Hardcoded one" : "Hardcoded two");
                        }
                    }
                    """,
                ).indented(),
            ).issues(HardcodedComposeStringDetector.ISSUE)
            .run()
            .expectWarningCount(2)
    }

    fun testTernaryBranchOnlyOneLiteralIsFlaggedOnce() {
        lint()
            .files(
                *stubs.toTypedArray(),
                java(
                    """
                    package com.example;
                    import static androidx.compose.material3.Material3.Text;
                    import static androidx.compose.material3.Material3.stringResource;
                    import androidx.compose.runtime.Composable;

                    public class Sample {
                        @Composable
                        public void sample(boolean cond) {
                            Text(cond ? "Hardcoded" : stringResource(1));
                        }
                    }
                    """,
                ).indented(),
            ).issues(HardcodedComposeStringDetector.ISSUE)
            .run()
            .expectWarningCount(1)
    }

    fun testCustomComposableNamedTitleParamIsFlagged() {
        lint()
            .files(
                *stubs.toTypedArray(),
                java(
                    """
                    package com.example;
                    import com.example.custom.CustomComponent;
                    import androidx.compose.runtime.Composable;

                    public class Sample {
                        @Composable
                        public void sample() {
                            CustomComponent.Render("Hardcoded title");
                        }
                    }
                    """,
                ).indented(),
            ).issues(HardcodedComposeStringDetector.ISSUE)
            .run()
            .expectWarningCount(1)
    }

    fun testNonComposableFunctionWithTitleParamIsClean() {
        lint()
            .files(
                *stubs.toTypedArray(),
                java(
                    """
                    package com.example;
                    import com.example.custom.CustomComponent;
                    import androidx.compose.runtime.Composable;

                    public class Sample {
                        @Composable
                        public void sample() {
                            CustomComponent.NotComposable("Not flagged");
                        }
                    }
                    """,
                ).indented(),
            ).issues(HardcodedComposeStringDetector.ISSUE)
            .run()
            .expectClean()
    }

    fun testTemplateWithLiteralWordIsFlagged() {
        lint()
            .files(
                *stubs.toTypedArray(),
                java(
                    """
                    package com.example;
                    import static androidx.compose.material3.Material3.Text;
                    import static androidx.compose.material3.Material3.stringResource;
                    import androidx.compose.runtime.Composable;
                    import androidx.compose.foundation.Foundation;

                    public class Sample {
                        @Composable
                        public void sample(int n) {
                            Text("Found " + n + " devices");
                        }
                    }
                    """,
                ).indented(),
            ).issues(HardcodedComposeStringDetector.ISSUE)
            .run()
            .expectWarningCount(1)
    }

    fun testTemplateOfSymbolsIsClean() {
        lint()
            .files(
                *stubs.toTypedArray(),
                java(
                    """
                    package com.example;
                    import static androidx.compose.material3.Material3.Text;
                    import static androidx.compose.material3.Material3.stringResource;
                    import androidx.compose.runtime.Composable;
                    import androidx.compose.foundation.Foundation;

                    public class Sample {
                        @Composable
                        public void sample(int n) {
                            Text("#" + n);
                        }
                    }
                    """,
                ).indented(),
            ).issues(HardcodedComposeStringDetector.ISSUE)
            .run()
            .expectClean()
    }

    fun testLiteralViaLocalVariableIsFlagged() {
        lint()
            .files(
                *stubs.toTypedArray(),
                java(
                    """
                    package com.example;
                    import static androidx.compose.material3.Material3.Text;
                    import static androidx.compose.material3.Material3.stringResource;
                    import androidx.compose.runtime.Composable;
                    import androidx.compose.foundation.Foundation;

                    public class Sample {
                        @Composable
                        public void sample(int n) {
                            String label = "Found " + n;
                            Text(label);
                        }
                    }
                    """,
                ).indented(),
            ).issues(HardcodedComposeStringDetector.ISSUE)
            // The literal sits away from the reported call, so the mode's suppression cannot reach it.
            .skipTestModes(TestMode.SUPPRESSIBLE)
            .run()
            .expectWarningCount(1)
    }

    fun testLiteralReturnedByHelperIsFlagged() {
        lint()
            .files(
                *stubs.toTypedArray(),
                java(
                    """
                    package com.example;
                    import static androidx.compose.material3.Material3.Text;
                    import static androidx.compose.material3.Material3.stringResource;
                    import androidx.compose.runtime.Composable;
                    import androidx.compose.foundation.Foundation;

                    public class Sample {
                        String label(boolean cond) {
                            return cond ? "Strong" : "Weak";
                        }

                        @Composable
                        public void sample(boolean cond) {
                            Text(label(cond));
                        }
                    }
                    """,
                ).indented(),
            ).issues(HardcodedComposeStringDetector.ISSUE)
            // The literal sits away from the reported call, so the mode's suppression cannot reach it.
            .skipTestModes(TestMode.SUPPRESSIBLE)
            .run()
            .expectWarningCount(2)
    }

    fun testHelperReturningStringResourceIsClean() {
        lint()
            .files(
                *stubs.toTypedArray(),
                java(
                    """
                    package com.example;
                    import static androidx.compose.material3.Material3.Text;
                    import static androidx.compose.material3.Material3.stringResource;
                    import androidx.compose.runtime.Composable;
                    import androidx.compose.foundation.Foundation;

                    public class Sample {
                        String label() {
                            return stringResource(1);
                        }

                        @Composable
                        public void sample() {
                            Text(label());
                        }
                    }
                    """,
                ).indented(),
            ).issues(HardcodedComposeStringDetector.ISSUE)
            .run()
            .expectClean()
    }

    fun testClickableOnClickLabelTemplateIsFlagged() {
        lint()
            .files(
                *stubs.toTypedArray(),
                java(
                    """
                    package com.example;
                    import static androidx.compose.material3.Material3.Text;
                    import static androidx.compose.material3.Material3.stringResource;
                    import androidx.compose.runtime.Composable;
                    import androidx.compose.foundation.Foundation;

                    public class Sample {
                        @Composable
                        public void sample(String name) {
                            Foundation.clickable("Connect to " + name);
                        }
                    }
                    """,
                ).indented(),
            ).issues(HardcodedComposeStringDetector.ISSUE)
            .run()
            .expectWarningCount(1)
    }
}
