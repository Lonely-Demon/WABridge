package com.wabridge.android.input

import android.accessibilityservice.AccessibilityService
import android.accessibilityservice.GestureDescription
import android.graphics.Path
import android.os.Handler
import android.os.Looper
import android.view.accessibility.AccessibilityEvent
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicBoolean

/**
 * Optional Phone Control adapter. Android users must enable this service
 * themselves in system settings; WABridge never attempts to enable it.
 */
class WABridgeAccessibilityService : AccessibilityService() {
    private val mainHandler = Handler(Looper.getMainLooper())

    override fun onServiceConnected() {
        super.onServiceConnected()
        AccessibilityInputBridge.attach(this)
    }

    override fun onAccessibilityEvent(event: AccessibilityEvent?) = Unit

    override fun onInterrupt() = Unit

    override fun onDestroy() {
        AccessibilityInputBridge.detach(this)
        mainHandler.removeCallbacksAndMessages(null)
        super.onDestroy()
    }

    fun dispatch(event: InputEvent): Boolean {
        if (!isOnMainThread()) {
            val accepted = AtomicBoolean(false)
            val completed = CountDownLatch(1)
            mainHandler.post {
                try {
                    accepted.set(dispatch(event))
                } finally {
                    completed.countDown()
                }
            }
            completed.await(1, TimeUnit.SECONDS)
            return accepted.get()
        }
        val metrics = resources.displayMetrics
        if (event.x !in 0 until metrics.widthPixels || event.y !in 0 until metrics.heightPixels) return false
        return when (event.type) {
            InputEventType.MOUSE_MOVE -> dispatchPoint(event.x.toFloat(), event.y.toFloat())
            InputEventType.MOUSE_BUTTON -> if (event.flags == 1) {
                dispatchPoint(event.x.toFloat(), event.y.toFloat())
            } else {
                false
            }
            InputEventType.MOUSE_WHEEL, InputEventType.KEY -> false
        }
    }

    private fun dispatchPoint(x: Float, y: Float): Boolean {
        val path = Path().apply { moveTo(x, y) }
        val gesture = GestureDescription.Builder()
            .addStroke(GestureDescription.StrokeDescription(path, 0, 1))
            .build()
        return dispatchGesture(gesture, null, mainHandler)
    }

    private fun isOnMainThread(): Boolean = Looper.myLooper() == Looper.getMainLooper()
}

object AccessibilityInputBridge {
    @Volatile
    private var service: WABridgeAccessibilityService? = null

    fun attach(candidate: WABridgeAccessibilityService) {
        service = candidate
    }

    fun detach(candidate: WABridgeAccessibilityService) {
        if (service === candidate) service = null
    }

    fun dispatch(event: InputEvent): Boolean = service?.dispatch(event) == true

    fun enabled(): Boolean = service != null
}
