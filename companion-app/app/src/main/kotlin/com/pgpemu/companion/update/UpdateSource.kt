package com.pgpemu.companion.update

import android.content.Context
import dagger.hilt.android.qualifiers.ApplicationContext
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.net.HttpURLConnection
import java.net.URL
import javax.inject.Inject

interface UpdateSource {
    /** Latest release tag, or null when unknown or already dismissed. */
    suspend fun latestTag(): String?

    fun dismiss(tag: String)
}

class GithubUpdateSource
    @Inject
    constructor(
        @ApplicationContext context: Context,
    ) : UpdateSource {
        private val prefs = context.getSharedPreferences("update_check", Context.MODE_PRIVATE)

        override suspend fun latestTag(): String? =
            withContext(Dispatchers.IO) {
                val now = System.currentTimeMillis()
                var tag = prefs.getString(KEY_TAG, null)
                if (now - prefs.getLong(KEY_LAST_CHECK, 0L) >= DAY_MS) {
                    // A failed request leaves the timestamp alone so the next launch retries.
                    fetchTag()?.let {
                        tag = it
                        prefs
                            .edit()
                            .putLong(KEY_LAST_CHECK, now)
                            .putString(KEY_TAG, it)
                            .apply()
                    }
                }
                tag?.takeIf { it != prefs.getString(KEY_DISMISSED, null) }
            }

        override fun dismiss(tag: String) {
            prefs.edit().putString(KEY_DISMISSED, tag).apply()
        }

        // Reading the redirect avoids a JSON parser and the API rate limit.
        private fun fetchTag(): String? =
            runCatching {
                val conn = URL(LATEST_URL).openConnection() as HttpURLConnection
                try {
                    conn.instanceFollowRedirects = false
                    conn.connectTimeout = TIMEOUT_MS
                    conn.readTimeout = TIMEOUT_MS
                    if (conn.responseCode != HttpURLConnection.HTTP_MOVED_TEMP) return@runCatching null
                    conn.getHeaderField("Location")?.substringAfterLast('/')?.takeIf { it.isNotEmpty() }
                } finally {
                    conn.disconnect()
                }
            }.getOrNull()

        private companion object {
            const val LATEST_URL = "https://github.com/shortcuts/pgpemu/releases/latest"
            const val KEY_LAST_CHECK = "last_check_ms"
            const val KEY_TAG = "latest_tag"
            const val KEY_DISMISSED = "dismissed_tag"
            const val DAY_MS = 24L * 60 * 60 * 1000
            const val TIMEOUT_MS = 10_000
        }
    }
