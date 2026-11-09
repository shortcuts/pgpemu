package com.pgpemu.companion.pairing

import android.content.Context
import android.provider.Settings
import dagger.hilt.android.qualifiers.ApplicationContext
import javax.inject.Inject

interface PairingServiceAccess {
    fun isEnabled(): Boolean
}

class AndroidPairingServiceAccess
    @Inject
    constructor(
        @ApplicationContext private val context: Context,
    ) : PairingServiceAccess {
        override fun isEnabled() =
            isServiceListed(
                Settings.Secure.getString(context.contentResolver, Settings.Secure.ENABLED_ACCESSIBILITY_SERVICES),
                context.packageName,
                PairingAssistantService::class.java.name,
            )
    }

/** Reads the `pkg/cls:pkg/.cls` list Android stores; ComponentName.unflattenFromString is a stub in JVM tests. */
fun isServiceListed(
    enabledServices: String?,
    packageName: String,
    className: String,
): Boolean {
    val full = "$packageName/$className"
    val short = "$packageName/${className.removePrefix(packageName)}"
    return enabledServices.orEmpty().split(':').any { it.equals(full, true) || it.equals(short, true) }
}
