package com.pgpemu.companion.update

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.StandardTestDispatcher
import kotlinx.coroutines.test.resetMain
import kotlinx.coroutines.test.runTest
import kotlinx.coroutines.test.setMain
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Test

@OptIn(ExperimentalCoroutinesApi::class)
class UpdateViewModelTest {
    private val dispatcher = StandardTestDispatcher()

    private class FakeSource(
        private val tag: String?,
    ) : UpdateSource {
        val dismissed = mutableListOf<String>()

        override suspend fun latestTag() = tag

        override fun dismiss(tag: String) {
            dismissed += tag
        }
    }

    @Before
    fun setUp() = Dispatchers.setMain(dispatcher)

    @After
    fun tearDown() = Dispatchers.resetMain()

    @Test
    fun `latestTag is filled after init and cleared by dismiss`() =
        runTest {
            val source = FakeSource("v1.0.2")
            val viewModel = UpdateViewModel(source)
            assertNull(viewModel.latestTag.value)

            dispatcher.scheduler.runCurrent()
            assertEquals("v1.0.2", viewModel.latestTag.value)

            viewModel.dismiss()
            assertNull(viewModel.latestTag.value)
            assertEquals(listOf("v1.0.2"), source.dismissed)
        }
}
