package com.pgpemu.companion.di

import com.pgpemu.companion.monitor.AndroidGameAlerts
import com.pgpemu.companion.monitor.AndroidNotificationAccess
import com.pgpemu.companion.monitor.GameAlerts
import com.pgpemu.companion.monitor.MonitorStore
import com.pgpemu.companion.monitor.NotificationAccess
import com.pgpemu.companion.monitor.SharedPreferencesMonitorStore
import dagger.Binds
import dagger.Module
import dagger.hilt.InstallIn
import dagger.hilt.components.SingletonComponent
import javax.inject.Singleton

@Module
@InstallIn(SingletonComponent::class)
abstract class MonitorModule {
    @Binds
    @Singleton
    abstract fun bindMonitorStore(impl: SharedPreferencesMonitorStore): MonitorStore

    @Binds
    @Singleton
    abstract fun bindNotificationAccess(impl: AndroidNotificationAccess): NotificationAccess

    @Binds
    @Singleton
    abstract fun bindGameAlerts(impl: AndroidGameAlerts): GameAlerts
}
