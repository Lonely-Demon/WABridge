package com.wabridge.android.discovery

import android.content.Context
import android.net.nsd.NsdManager
import android.net.nsd.NsdServiceInfo
import java.util.concurrent.atomic.AtomicBoolean

/**
 * Wi-Fi DNS-SD discovery for WABridge.
 *
 * Android normally acts as a client of the Windows coordinator, so callers
 * should use [startBrowsing] unless they also own a reachable TCP listener.
 * [start] is retained for the combined advertise-and-browse case.
 */
class NsdDiscovery(
    context: Context,
    private val fingerprint: () -> String,
    private val onCandidate: (NsdServiceInfo) -> Unit,
    private val onError: (String) -> Unit,
) {
    private val manager = context.getSystemService(NsdManager::class.java)
    private val stopped = AtomicBoolean(true)
    private var registrationListener: NsdManager.RegistrationListener? = null
    private var discoveryListener: NsdManager.DiscoveryListener? = null
    private var registered = false
    private var browsing = false

    /** Starts advertising this device and browsing for Windows coordinators. */
    fun start(port: Int) {
        require(port in 1..65535)
        ensureRunning()
        startAdvertisingInternal(port)
        startBrowsingInternal()
    }

    /** Starts browsing only; no local TCP listener or fake advertisement is required. */
    fun startBrowsing() {
        ensureRunning()
        startBrowsingInternal()
    }

    /** Starts advertising only for a caller that owns a reachable TCP listener. */
    fun startAdvertising(port: Int) {
        require(port in 1..65535)
        ensureRunning()
        startAdvertisingInternal(port)
    }

    private fun ensureRunning() {
        stopped.compareAndSet(true, false)
    }

    private fun startAdvertisingInternal(port: Int) {
        if (registrationListener != null || stopped.get()) return
        val serviceInfo = NsdServiceInfo().apply {
            serviceName = "WABridge Android"
            serviceType = SERVICE_TYPE
            this.port = port
            setAttribute("version", "1")
            setAttribute("role", "android")
            setAttribute("fingerprint", fingerprint().take(95))
        }
        registrationListener = object : NsdManager.RegistrationListener {
            override fun onServiceRegistered(info: NsdServiceInfo) { registered = true }
            override fun onRegistrationFailed(info: NsdServiceInfo, errorCode: Int) {
                registered = false
                onError("NSD registration failed: $errorCode")
            }
            override fun onServiceUnregistered(info: NsdServiceInfo) { registered = false }
            override fun onUnregistrationFailed(info: NsdServiceInfo, errorCode: Int) {
                onError("NSD unregistration failed: $errorCode")
            }
        }
        runCatching {
            manager.registerService(serviceInfo, NsdManager.PROTOCOL_DNS_SD, registrationListener)
        }.onFailure {
            registrationListener = null
            onError("NSD registration failed: ${it.message ?: "unknown error"}")
        }
    }

    private fun startBrowsingInternal() {
        if (discoveryListener != null || stopped.get()) return
        discoveryListener = object : NsdManager.DiscoveryListener {
            override fun onDiscoveryStarted(type: String) { browsing = true }
            override fun onDiscoveryStopped(type: String) { browsing = false }
            override fun onStartDiscoveryFailed(type: String, errorCode: Int) {
                browsing = false
                onError("NSD discovery failed: $errorCode")
            }
            override fun onStopDiscoveryFailed(type: String, errorCode: Int) {
                browsing = false
                onError("NSD stop failed: $errorCode")
            }
            override fun onServiceFound(info: NsdServiceInfo) {
                if (info.serviceType == SERVICE_TYPE) resolve(info)
            }
            override fun onServiceLost(info: NsdServiceInfo) = Unit
        }
        runCatching {
            manager.discoverServices(SERVICE_TYPE, NsdManager.PROTOCOL_DNS_SD, discoveryListener)
        }.onFailure {
            discoveryListener = null
            onError("NSD discovery failed: ${it.message ?: "unknown error"}")
        }
    }

    private fun resolve(candidate: NsdServiceInfo) {
        if (stopped.get()) return
        runCatching {
            manager.resolveService(candidate, object : NsdManager.ResolveListener {
                override fun onResolveFailed(info: NsdServiceInfo, errorCode: Int) {
                    onError("NSD resolve failed: $errorCode")
                }
                override fun onServiceResolved(info: NsdServiceInfo) {
                    if (!stopped.get() && info.port in 1..65535) onCandidate(info)
                }
            })
        }.onFailure {
            onError("NSD resolve failed: ${it.message ?: "unknown error"}")
        }
    }

    fun stop() {
        if (!stopped.compareAndSet(false, true)) return
        discoveryListener?.let { listener ->
            if (browsing) runCatching { manager.stopServiceDiscovery(listener) }
        }
        registrationListener?.let { listener ->
            if (registered) runCatching { manager.unregisterService(listener) }
        }
        browsing = false
        registered = false
        discoveryListener = null
        registrationListener = null
    }

    companion object {
        const val SERVICE_TYPE = "_wabridge._tcp."
    }
}
