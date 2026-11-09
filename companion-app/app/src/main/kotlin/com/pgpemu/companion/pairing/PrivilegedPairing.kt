// Derived from PoGo+LE (https://github.com/Mygod/pogoplusle) by Mygod, Apache License 2.0. Modified for pgpemu: the receiver's confirm decision moved into a pure function so JVM tests drive it.
package com.pgpemu.companion.pairing

enum class PrivilegedOutcome { Ignored, Confirmed, Failed }

// The platform constant for the consent dialog (with the contacts switch) is hidden.
const val PAIRING_VARIANT_CONSENT = 3

/** Confirms the consent dialog; every failure leaves Android's normal dialog in place. */
fun confirmPrivileged(
    variant: Int,
    isPgp: Boolean,
    direct: () -> Boolean,
    viaRoot: () -> Boolean,
): PrivilegedOutcome {
    if (variant != PAIRING_VARIANT_CONSENT || !isPgp) return PrivilegedOutcome.Ignored
    val directly =
        try {
            direct()
        } catch (_: SecurityException) {
            false
        }
    val confirmed = directly || runCatching(viaRoot).getOrDefault(false)
    return if (confirmed) PrivilegedOutcome.Confirmed else PrivilegedOutcome.Failed
}
