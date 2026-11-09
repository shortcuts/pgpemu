package com.pgpemu.companion.ble

private const val NAME_FIELD_SIZE = 16
private const val MAC_SIZE = 6
private const val KEY_SIZE = 16
private const val BLOB_SIZE = 256

/** The four values of the firmware's secrets.csv, in the PROVISION_SECRETS payload layout. */
class SecretsImport(
    val name: String,
    val mac: ByteArray,
    val key: ByteArray,
    val blob: ByteArray,
) {
    /** name[16] NUL-padded, mac[6], dkey[16], blob[256]: the GET_SECRETS layout. */
    fun toPayload(): ByteArray = name.toByteArray(Charsets.US_ASCII).copyOf(NAME_FIELD_SIZE) + mac + key + blob
}

private fun hex(
    row: String,
    value: String,
    size: Int,
): ByteArray {
    if (value.length != size * 2 || !value.all { it in '0'..'9' || it in 'a'..'f' || it in 'A'..'F' }) {
        throw DecodeError("$row: expected $size bytes of hex")
    }
    return ByteArray(size) { value.substring(it * 2, it * 2 + 2).toInt(16).toByte() }
}

/** Reads `key,type,encoding,value` rows named name, mac, dkey and blob; the same checks as the firmware. */
fun parseSecretsCsv(text: String): Result<SecretsImport> =
    runCatching {
        val rows =
            text
                .lineSequence()
                .map { it.trim() }
                .filter { it.isNotEmpty() && !it.startsWith("#") }
                .map { it.split(",", limit = 4) }
                .filter { it.size == 4 }
                .associate { it[0].trim() to it[3].trim().removeSurrounding("\"") }

        fun row(key: String) = rows[key] ?: throw DecodeError("$key: row missing")
        val name = row("name")
        if (name.isEmpty() || name.length >= NAME_FIELD_SIZE || !name.all { it in ' '..'~' }) {
            throw DecodeError("name: expected 1 to 15 printable ASCII characters")
        }
        val mac = hex("mac", row("mac"), MAC_SIZE)
        if (mac.all { it == 0.toByte() }) throw DecodeError("mac: all zero")
        SecretsImport(name, mac, hex("dkey", row("dkey"), KEY_SIZE), hex("blob", row("blob"), BLOB_SIZE))
    }
