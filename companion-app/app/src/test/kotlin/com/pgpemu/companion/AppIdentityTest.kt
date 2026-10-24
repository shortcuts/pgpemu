package com.pgpemu.companion

import org.junit.Assert.assertEquals
import org.junit.Test
import org.w3c.dom.Element
import java.io.File
import javax.xml.parsers.DocumentBuilderFactory

class AppIdentityTest {
    private val manifest = xml("src/main/AndroidManifest.xml")
    private val strings = xml("src/main/res/values/strings.xml")

    @Test
    fun `launcher label is the product name`() {
        val label = application().getAttribute("android:label")
        val string =
            strings
                .getElementsByTagName("string")
                .let { nodes ->
                    (0 until nodes.length).map { nodes.item(it) as Element }
                }.single { it.getAttribute("name") == label.removePrefix("@string/") }
        assertEquals("PGPemu Companion", string.textContent)
    }

    @Test
    fun `bluetooth low energy is required`() {
        val feature = manifest.getElementsByTagName("uses-feature").item(0) as Element
        assertEquals("android.hardware.bluetooth_le", feature.getAttribute("android:name"))
        assertEquals("true", feature.getAttribute("android:required"))
    }

    @Test
    fun `application uses the day-night startup theme`() {
        assertEquals("@style/Theme.PgpEmu.Startup", application().getAttribute("android:theme"))
    }

    private fun application() = manifest.getElementsByTagName("application").item(0) as Element

    private fun xml(path: String) =
        DocumentBuilderFactory
            .newInstance()
            .newDocumentBuilder()
            .parse(File(path))
            .documentElement
}
