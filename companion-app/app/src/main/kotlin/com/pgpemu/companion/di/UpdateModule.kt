package com.pgpemu.companion.di

import com.pgpemu.companion.update.GithubUpdateSource
import com.pgpemu.companion.update.SharedPreferencesWhatsNewStore
import com.pgpemu.companion.update.UpdateSource
import com.pgpemu.companion.update.WhatsNewStore
import dagger.Binds
import dagger.Module
import dagger.hilt.InstallIn
import dagger.hilt.components.SingletonComponent
import javax.inject.Singleton

@Module
@InstallIn(SingletonComponent::class)
abstract class UpdateModule {
    @Binds
    @Singleton
    abstract fun bindUpdateSource(impl: GithubUpdateSource): UpdateSource

    @Binds
    @Singleton
    abstract fun bindWhatsNewStore(impl: SharedPreferencesWhatsNewStore): WhatsNewStore
}
