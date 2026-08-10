package com.pgpemu.companion.di

import com.pgpemu.companion.ble.BleControlRepository
import com.pgpemu.companion.ble.NordicBleControlRepository
import dagger.Binds
import dagger.Module
import dagger.hilt.InstallIn
import dagger.hilt.components.SingletonComponent
import javax.inject.Singleton

@Module
@InstallIn(SingletonComponent::class)
abstract class BleModule {

    @Binds
    @Singleton
    abstract fun bindBleControlRepository(impl: NordicBleControlRepository): BleControlRepository
}
