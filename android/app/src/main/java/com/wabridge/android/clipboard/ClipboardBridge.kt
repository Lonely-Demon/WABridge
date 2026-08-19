package com.wabridge.android.clipboard

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import java.util.concurrent.atomic.AtomicBoolean

class ClipboardBridge(
    context: Context,
    private val localDeviceId: String,
    private val onLocalUpdate: (ClipboardUpdate) -> Unit,
    private val onError: (String) -> Unit,
) {
    private val appContext = context.applicationContext
    private val clipboard = context.getSystemService(ClipboardManager::class.java)
    private val active = AtomicBoolean(false)
    private val guard = ClipboardLoopGuard()

    private val listener = ClipboardManager.OnPrimaryClipChangedListener {
        if (!active.get()) return@OnPrimaryClipChangedListener
        runCatching { readLocalUpdate() }
            .onSuccess { update -> if (guard.shouldApply(update.loopToken)) onLocalUpdate(update) }
            .onFailure { error -> onError(error.message ?: "clipboard read failed") }
    }

    fun start() {
        if (!active.compareAndSet(false, true)) return
        clipboard.addPrimaryClipChangedListener(listener)
    }

    fun applyRemote(update: ClipboardUpdate) {
        if (!active.get()) return
        if (!guard.shouldApply(update.loopToken)) return
        clipboard.setPrimaryClip(ClipData.newPlainText("WABridge", update.text))
    }

    fun stop() {
        if (!active.compareAndSet(true, false)) return
        clipboard.removePrimaryClipChangedListener(listener)
    }

    private fun readLocalUpdate(): ClipboardUpdate {
        val clip = clipboard.primaryClip ?: error("clipboard is empty")
        if (clip.itemCount == 0) error("clipboard has no items")
        val text = clip.getItemAt(0).coerceToText(appContext).toString()
        if (text.isEmpty() || text.toByteArray(Charsets.UTF_8).size > ClipboardCodec.MAX_TEXT) {
            error("clipboard text is too large")
        }
        val token = java.security.MessageDigest.getInstance("SHA-256")
            .digest(text.toByteArray(Charsets.UTF_8)).copyOf(16)
        guard.markLocal(token)
        return ClipboardUpdate(token, localDeviceId, System.currentTimeMillis(), text)
    }
}
