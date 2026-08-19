#include "wabridge_coordinator.h"

#include <utility>

namespace wabridge::coordinator {

Coordinator::Coordinator(AcceptHandler handler) : handler_(std::move(handler)) {}

Coordinator::~Coordinator() {
    stop();
}

bool Coordinator::start(const std::uint16_t port) {
    std::lock_guard lock(mutex_);
    if (running_.load()) return false;
    auto listener = net::Socket::listen(port);
    if (!listener.has_value()) return false;
    listener_ = std::move(listener);
    running_.store(true);
    thread_ = std::thread(&Coordinator::accept_loop, this);
    return true;
}

void Coordinator::stop() noexcept {
    {
        std::lock_guard lock(mutex_);
        if (!running_.exchange(false)) return;
        if (listener_.has_value()) listener_->close();
    }
    if (thread_.joinable()) thread_.join();
    std::lock_guard lock(mutex_);
    listener_.reset();
}

std::uint16_t Coordinator::port() const noexcept {
    std::lock_guard lock(mutex_);
    return listener_.has_value() ? listener_->local_port() : 0;
}

void Coordinator::accept_loop() {
    while (running_.load()) {
        std::optional<net::Socket> accepted;
        {
            std::lock_guard lock(mutex_);
            if (!listener_.has_value()) return;
            accepted = listener_->accept();
        }
        if (!accepted.has_value()) {
            if (running_.load()) continue;
            return;
        }
        if (handler_) {
            handler_(std::move(*accepted));
        }
    }
}

} // namespace wabridge::coordinator
