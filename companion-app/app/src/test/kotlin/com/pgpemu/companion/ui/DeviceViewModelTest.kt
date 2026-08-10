package com.pgpemu.companion.ui

import com.pgpemu.companion.ble.ConnectionState
import com.pgpemu.companion.core.testing.FakeBleControlRepository
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.StandardTestDispatcher
import kotlinx.coroutines.test.resetMain
import kotlinx.coroutines.test.runTest
import kotlinx.coroutines.test.setMain
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test

@OptIn(ExperimentalCoroutinesApi::class)
class DeviceViewModelTest {

    private val dispatcher = StandardTestDispatcher()

    @Before
    fun setUp() {
        Dispatchers.setMain(dispatcher)
    }

    @After
    fun tearDown() {
        Dispatchers.resetMain()
    }

    @Test
    fun `uiState reflects repository connection state`() = runTest {
        val repository = FakeBleControlRepository()
        val viewModel = DeviceViewModel(repository)

        repository.setConnectionState(ConnectionState.Ready)
        dispatcher.scheduler.advanceUntilIdle()

        assertEquals(ConnectionState.Ready, viewModel.uiState.value.connectionState)
    }
}
