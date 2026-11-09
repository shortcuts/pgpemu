package com.pgpemu.companion.di

import com.pgpemu.companion.pairing.AndroidPairingAlerts
import com.pgpemu.companion.pairing.AndroidPairingServiceAccess
import com.pgpemu.companion.pairing.AndroidPrivilegedPairingAccess
import com.pgpemu.companion.pairing.PairingAlerts
import com.pgpemu.companion.pairing.PairingServiceAccess
import com.pgpemu.companion.pairing.PairingStore
import com.pgpemu.companion.pairing.PrivilegedPairingAccess
import com.pgpemu.companion.pairing.SharedPreferencesPairingStore
import dagger.Binds
import dagger.Module
import dagger.hilt.InstallIn
import dagger.hilt.components.SingletonComponent
import javax.inject.Singleton

@Module
@InstallIn(SingletonComponent::class)
abstract class PairingModule {
    @Binds
    @Singleton
    abstract fun bindPairingStore(impl: SharedPreferencesPairingStore): PairingStore

    @Binds
    @Singleton
    abstract fun bindPairingServiceAccess(impl: AndroidPairingServiceAccess): PairingServiceAccess

    @Binds
    @Singleton
    abstract fun bindPrivilegedPairingAccess(impl: AndroidPrivilegedPairingAccess): PrivilegedPairingAccess

    @Binds
    @Singleton
    abstract fun bindPairingAlerts(impl: AndroidPairingAlerts): PairingAlerts
}
