package com.pgpemu.companion.pairing

import com.pgpemu.companion.core.testing.FakePairingServiceAccess
import com.pgpemu.companion.core.testing.FakePairingStore
import com.pgpemu.companion.core.testing.FakePrivilegedPairingAccess
import org.junit.Assert.assertEquals
import org.junit.Test

class PairingAssistantViewModelTest {
    private val store = FakePairingStore()
    private val service = FakePairingServiceAccess()
    private val privileged = FakePrivilegedPairingAccess()
    private val viewModel = PairingAssistantViewModel(store, service, privileged)

    @Test
    fun `refresh picks up the accessibility service state`() {
        assertEquals(PairingAssistantState(serviceEnabled = false, privilegedEnabled = false), viewModel.state.value)
        service.enabled = true
        viewModel.refresh()
        assertEquals(true, viewModel.state.value.serviceEnabled)
    }

    @Test
    fun `granted root writes the store and enables the receiver`() {
        privileged.grantOnRequest = true
        viewModel.setPrivileged(true)
        assertEquals(1, privileged.requests)
        assertEquals(true, store.privilegedEnabled)
        assertEquals(true, privileged.receiverOn)
    }

    @Test
    fun `denied root changes nothing`() {
        viewModel.setPrivileged(true)
        assertEquals(false, store.privilegedEnabled)
        assertEquals(false, privileged.receiverOn)
    }

    @Test
    fun `turning off disables the receiver without asking`() {
        privileged.grantOnRequest = true
        viewModel.setPrivileged(true)
        viewModel.setPrivileged(false)
        assertEquals(1, privileged.requests)
        assertEquals(false, store.privilegedEnabled)
        assertEquals(false, privileged.receiverOn)
    }
}
