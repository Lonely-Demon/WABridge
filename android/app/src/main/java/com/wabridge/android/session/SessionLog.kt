package com.wabridge.android.session

import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/** Bounded, UI-safe connection log retained for the current app process. */
data class SessionLogEntry(
    val timestamp: Long,
    val level: Level,
    val message: String,
) {
    enum class Level { INFO, WARN, ERROR }

    fun displayTime(): String = SimpleDateFormat("HH:mm:ss", Locale.US).format(Date(timestamp))
}

object SessionLog {
    private const val MAX_ENTRIES = 200
    private val _entries = MutableStateFlow<List<SessionLogEntry>>(emptyList())
    val entries: StateFlow<List<SessionLogEntry>> = _entries.asStateFlow()

    fun info(message: String) = append(SessionLogEntry.Level.INFO, message)
    fun warn(message: String) = append(SessionLogEntry.Level.WARN, message)
    fun error(message: String) = append(SessionLogEntry.Level.ERROR, message)

    fun clear() {
        _entries.value = emptyList()
    }

    private fun append(level: SessionLogEntry.Level, message: String) {
        val clean = message.trim().take(500)
        if (clean.isEmpty()) return
        val entry = SessionLogEntry(System.currentTimeMillis(), level, clean)
        _entries.value = (_entries.value + entry).takeLast(MAX_ENTRIES)
    }
}
