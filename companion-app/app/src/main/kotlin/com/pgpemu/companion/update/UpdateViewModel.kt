package com.pgpemu.companion.update

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import dagger.hilt.android.lifecycle.HiltViewModel
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.launch
import javax.inject.Inject

@HiltViewModel
class UpdateViewModel
    @Inject
    constructor(
        private val source: UpdateSource,
    ) : ViewModel() {
        private val _latestTag = MutableStateFlow<String?>(null)
        val latestTag: StateFlow<String?> = _latestTag

        init {
            viewModelScope.launch { _latestTag.value = source.latestTag() }
        }

        fun dismiss() {
            _latestTag.value?.let(source::dismiss)
            _latestTag.value = null
        }
    }
