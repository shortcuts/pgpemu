package com.pgpemu.companion.monitor

import com.pgpemu.companion.core.testing.FakeMonitorStore
import com.pgpemu.companion.core.testing.FakeNotificationAccess
import org.junit.Assert.assertEquals
import org.junit.Test

class GameMonitorViewModelTest {
    private val store = FakeMonitorStore()
    private val access = FakeNotificationAccess()
    private val viewModel = GameMonitorViewModel(store, access)

    @Test
    fun `initial state comes from the store and access`() {
        assertEquals(GameMonitorState(enabled = false, accessGranted = false), viewModel.state.value)
    }

    @Test
    fun `setEnabled writes the store and updates the state`() {
        viewModel.setEnabled(true)
        assertEquals(true, store.enabled)
        assertEquals(true, viewModel.state.value.enabled)
    }

    @Test
    fun `refresh picks up a changed access grant`() {
        access.granted = true
        assertEquals(false, viewModel.state.value.accessGranted)
        viewModel.refresh()
        assertEquals(true, viewModel.state.value.accessGranted)
    }
}
