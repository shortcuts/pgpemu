package com.pgpemu.companion.update

import com.pgpemu.companion.BuildConfig
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class WhatsNewViewModelTest {
    private class FakeStore(
        override var seenVersion: String?,
    ) : WhatsNewStore

    @Test
    fun `due only when a different version was seen before`() {
        assertFalse(whatsNewDue(null, "1.1.0"))
        assertFalse(whatsNewDue("1.1.0", "1.1.0"))
        assertTrue(whatsNewDue("1.0.1", "1.1.0"))
    }

    @Test
    fun `fresh install records the version and stays hidden`() {
        val store = FakeStore(null)
        val viewModel = WhatsNewViewModel(store)
        assertFalse(viewModel.visible.value)
        assertEquals(BuildConfig.VERSION_NAME, store.seenVersion)
    }

    @Test
    fun `update shows once and dismiss records the version`() {
        val store = FakeStore("0.0.1")
        val viewModel = WhatsNewViewModel(store)
        assertTrue(viewModel.visible.value)
        viewModel.dismiss()
        assertFalse(viewModel.visible.value)
        assertEquals(BuildConfig.VERSION_NAME, store.seenVersion)
        assertFalse(WhatsNewViewModel(store).visible.value)
    }
}
