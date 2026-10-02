package com.pgpemu.companion.lint.checks

import com.android.tools.lint.client.api.UElementHandler
import com.android.tools.lint.detector.api.Category
import com.android.tools.lint.detector.api.ConstantEvaluator
import com.android.tools.lint.detector.api.Detector
import com.android.tools.lint.detector.api.Implementation
import com.android.tools.lint.detector.api.Issue
import com.android.tools.lint.detector.api.JavaContext
import com.android.tools.lint.detector.api.Scope
import com.android.tools.lint.detector.api.Severity
import org.jetbrains.uast.UBinaryExpression
import org.jetbrains.uast.UCallExpression
import org.jetbrains.uast.UElement
import org.jetbrains.uast.UExpression
import org.jetbrains.uast.UExpressionList
import org.jetbrains.uast.UIfExpression
import org.jetbrains.uast.UMethod
import org.jetbrains.uast.USimpleNameReferenceExpression
import org.jetbrains.uast.USwitchClauseExpressionWithBody
import org.jetbrains.uast.USwitchExpression
import org.jetbrains.uast.UYieldExpression
import org.jetbrains.uast.UastBinaryOperator
import org.jetbrains.uast.getParentOfType
import org.jetbrains.uast.kotlin.kinds.KotlinSpecialExpressionKinds
import org.jetbrains.uast.skipParenthesizedExprDown

/**
 * Flags a literal `String` passed to the text-bearing parameter of a small,
 * fixed set of Compose/Material calls. Deliberately narrow: it only flags a
 * compile-time-constant string — `ConstantEvaluator` resolves a plain Kotlin
 * string literal (which UAST wraps as `KotlinStringTemplateUPolyadicExpression`
 * even with zero `$` interpolation, not a bare `ULiteralExpression`) but
 * returns null for a template with an interpolated (non-constant) part, so a
 * string like `"Hello $name"` is not flagged — the extraction sweep turns
 * those into `stringResource(R.string.x, arg)` calls, which are themselves
 * calls, not literals, and so are already clean.
 */
class HardcodedComposeStringDetector :
    Detector(),
    Detector.UastScanner {
    override fun getApplicableUastTypes() = listOf(UCallExpression::class.java, UBinaryExpression::class.java)

    override fun createUastHandler(context: JavaContext): UElementHandler =
        object : UElementHandler() {
            override fun visitCallExpression(node: UCallExpression) {
                val method = node.resolve() ?: return
                if (isInsidePreview(node)) return

                val target = TARGETS[node.methodName]
                val containingClass = method.containingClass?.qualifiedName
                val paramNames: Set<String> =
                    when {
                        target != null &&
                            containingClass != null &&
                            target.classPrefixes.any { containingClass.startsWith(it) } -> setOf(target.paramName)
                        // animateFloatAsState/Crossfade/rememberInfiniteTransition etc. take a `label`
                        // param that's an Android Studio animation-inspector tag, never user-facing text.
                        containingClass != null && containingClass.startsWith("androidx.compose.animation") -> return
                        context.evaluator.findAnnotation(method, COMPOSABLE_ANNOTATION) != null -> GENERIC_PARAM_NAMES
                        else -> return
                    }

                val mapping = context.evaluator.computeArgumentMapping(node, method)
                mapping.forEach { (argumentExpression, parameter) ->
                    if (parameter.name !in paramNames) return@forEach
                    reportHardcodedLeaves(
                        context,
                        node,
                        argumentExpression,
                        "Hardcoded string passed to ${node.methodName}() — use stringResource() instead",
                    )
                }
            }

            override fun visitBinaryExpression(node: UBinaryExpression) {
                if (node.operator != UastBinaryOperator.ASSIGN) return
                val leftName =
                    (node.leftOperand.skipParenthesizedExprDown() as? USimpleNameReferenceExpression)?.identifier
                if (leftName != "contentDescription") return
                if (!isInsideSemanticsLambda(node)) return
                if (isInsidePreview(node)) return
                reportHardcodedLeaves(
                    context,
                    node,
                    node.rightOperand,
                    "Hardcoded string assigned to contentDescription — use stringResource() instead",
                )
            }
        }

    private fun isInsidePreview(node: UElement): Boolean {
        val method = node.getParentOfType(UMethod::class.java) ?: return false
        return method.uAnnotations.any { it.qualifiedName?.substringAfterLast('.') == "Preview" }
    }

    private fun isInsideSemanticsLambda(node: UElement): Boolean {
        val enclosingCall = node.getParentOfType(UCallExpression::class.java) ?: return false
        return enclosingCall.methodName == "semantics"
    }

    private fun collectLeafExpressions(
        expression: UExpression,
        into: MutableList<UExpression>,
    ) {
        val expr = expression.skipParenthesizedExprDown()
        when {
            expr is UIfExpression -> {
                expr.thenExpression?.let { collectLeafExpressions(it, into) }
                expr.elseExpression?.let { collectLeafExpressions(it, into) }
            }
            expr is UExpressionList && expr.kind == KotlinSpecialExpressionKinds.ELVIS -> {
                expr.expressions.forEach { collectLeafExpressions(it, into) }
            }
            expr is USwitchExpression -> {
                expr.body.expressions
                    .filterIsInstance<USwitchClauseExpressionWithBody>()
                    .forEach { clause ->
                        clause.body.expressions
                            .lastOrNull()
                            ?.let { collectLeafExpressions(it, into) }
                    }
            }
            expr is UYieldExpression -> {
                expr.expression?.let { collectLeafExpressions(it, into) }
            }
            else -> into.add(expr)
        }
    }

    private fun reportHardcodedLeaves(
        context: JavaContext,
        reportNode: UElement,
        argumentExpression: UExpression,
        message: String,
    ) {
        val leaves = mutableListOf<UExpression>()
        collectLeafExpressions(argumentExpression, leaves)
        for (leaf in leaves) {
            val value = ConstantEvaluator.evaluate(context, leaf) as? String ?: continue
            if (value.isEmpty()) continue
            context.report(ISSUE, reportNode, context.getLocation(leaf), message)
        }
    }

    private data class Target(
        val paramName: String,
        val classPrefixes: List<String>,
    )

    companion object {
        private const val COMPOSABLE_ANNOTATION = "androidx.compose.runtime.Composable"
        private val GENERIC_PARAM_NAMES =
            setOf("contentDescription", "title", "label", "text", "placeholder")

        private val TARGETS =
            mapOf(
                "Text" to
                    Target("text", listOf("androidx.compose.material3.", "androidx.compose.material.")),
                "Icon" to
                    Target(
                        "contentDescription",
                        listOf("androidx.compose.material3.", "androidx.compose.material."),
                    ),
                "Image" to
                    Target("contentDescription", listOf("androidx.compose.foundation.")),
                "showSnackbar" to
                    Target("message", listOf("androidx.compose.material3.", "androidx.compose.material.")),
            )

        val ISSUE: Issue =
            Issue.create(
                id = "HardcodedComposeString",
                briefDescription = "Hardcoded user-facing string in Compose UI",
                explanation =
                    """
                        User-facing text must come from a string resource so the app can be \
                        localized later. Move this literal into the owning module's \
                        `res/values/strings.xml` and reference it via `stringResource(R.string.xxx)` \
                        (or `context.getString(...)` outside a composable).
                        """,
                category = Category.I18N,
                priority = 6,
                severity = Severity.WARNING,
                implementation = Implementation(HardcodedComposeStringDetector::class.java, Scope.JAVA_FILE_SCOPE),
            )
    }
}
