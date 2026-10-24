package com.pgpemu.companion.ble

object EventType {
    const val GAME_CONNECTED: Int = 0x01
    const val GAME_DISCONNECTED: Int = 0x02
    const val POKEMON_IN_RANGE: Int = 0x03
    const val NEW_POKEMON_IN_RANGE: Int = 0x04
    const val POKESTOP_IN_RANGE: Int = 0x05
    const val CAUGHT: Int = 0x06
    const val FLED: Int = 0x07
    const val ITEMS_RECEIVED: Int = 0x08
    const val BAG_FULL: Int = 0x09
    const val BOX_FULL: Int = 0x0A
    const val BALLS_EMPTY: Int = 0x0B
}
