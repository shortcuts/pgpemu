package com.pgpemu.companion.di

import com.pgpemu.companion.ui.theme.SharedPreferencesThemeStore
import com.pgpemu.companion.ui.theme.ThemeStore
import dagger.Binds
import dagger.Module
import dagger.hilt.InstallIn
import dagger.hilt.components.SingletonComponent
import javax.inject.Singleton

@Module
@InstallIn(SingletonComponent::class)
abstract class ThemeModule {
    @Binds
    @Singleton
    abstract fun bindThemeStore(impl: SharedPreferencesThemeStore): ThemeStore
}
