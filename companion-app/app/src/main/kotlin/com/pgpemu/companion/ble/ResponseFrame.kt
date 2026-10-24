package com.pgpemu.companion.ble

data class ResponseFrame(
    val status: Byte,
    val opcode: Byte,
    val payload: ByteArray,
) {
    val isOk: Boolean get() = status == StatusCode.OK

    override fun equals(other: Any?): Boolean =
        other is ResponseFrame &&
            status == other.status &&
            opcode == other.opcode &&
            payload.contentEquals(other.payload)

    /** The firmware echoes the request opcode; any other frame is stale and must not complete the request. */
    fun answers(requestOpcode: Int): Boolean = (opcode.toInt() and 0xFF) == (requestOpcode and 0xFF)

    override fun hashCode(): Int = 31 * (31 * status + opcode) + payload.contentHashCode()
}

object StatusCode {
    const val OK: Byte = 0x00
    const val ERR_UNKNOWN_OPCODE: Byte = 0x01
    const val ERR_MALFORMED_PAYLOAD: Byte = 0x02
    const val ERR_NOT_BONDED: Byte = 0x03
    const val ERR_BUSY: Byte = 0x04
    const val ERR_INTERNAL: Byte = 0x05
    const val ERR_NOT_PROVISIONING: Byte = 0x06
    const val ERR_OTA_NOT_ARMED: Byte = 0x07
    const val ERR_OTA_STATE: Byte = 0x08
    const val ERR_NOT_FOUND: Byte = 0x09
}
