package com.pgpemu.companion.ble

object Opcode {
    const val HELP: Int = 0x01
    const val GET_GLOBAL_SETTINGS: Int = 0x02
    const val SAVE_SETTINGS: Int = 0x03
    const val GET_SECRETS: Int = 0x04
    const val RESET_SECRETS: Int = 0x05
    const val RESTART: Int = 0x06
    const val GET_LED_STATE: Int = 0x07
    const val CYCLE_LOG_LEVEL: Int = 0x08
    const val GET_RUNTIME_STATS: Int = 0x09
    const val GET_TASK_LIST: Int = 0x0A
    const val ADVERTISE_START: Int = 0x0B
    const val ADVERTISE_STOP: Int = 0x0C
    const val GET_CLIENT_STATES: Int = 0x0D
    const val RESET_CLIENT_STATES: Int = 0x0E
    const val SET_MAX_CONNECTIONS: Int = 0x0F
    const val TOGGLE_AUTOSPIN: Int = 0x10
    const val TOGGLE_AUTOCATCH: Int = 0x11
    const val GET_CLIENT_SUMMARY: Int = 0x12
}
