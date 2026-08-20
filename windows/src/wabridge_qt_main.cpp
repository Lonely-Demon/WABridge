#include "wabridge_messages.h"
#include "wabridge_feature_dispatch.h"
#include "wabridge_identity.h"
#include "wabridge_input.h"
#include "wabridge_input_capture.h"
#include "wabridge_wasapi_audio.h"
#include "wabridge_secure_coordinator.h"
#include "wabridge_tls.h"

#include <QApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMetaObject>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>

namespace {

std::string read_file(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    const QByteArray contents = file.readAll();
    return {contents.constData(), static_cast<std::size_t>(contents.size())};
}

class CoordinatorWindow final : public QMainWindow {
public:
    CoordinatorWindow() {
        setWindowTitle("WABridge — Windows coordinator");
        resize(620, 360);

        auto* root = new QWidget(this);
        auto* layout = new QVBoxLayout(root);
        auto* form = new QFormLayout();

        cert_path_ = new QLineEdit(root);
        key_path_ = new QLineEdit(root);
        cert_path_->setPlaceholderText("Optional PEM certificate override");
        key_path_->setPlaceholderText("Optional PEM private-key override");
        form->addRow("Device certificate:", with_browse(cert_path_, "Select certificate"));
        form->addRow("Private key:", with_browse(key_path_, "Select private key"));

        port_ = new QSpinBox(root);
        port_->setRange(1, 65535);
        port_->setValue(51820);
        form->addRow("TCP port:", port_);
        layout->addLayout(form);

        status_ = new QLabel("Not running — a DPAPI-protected device identity will be created automatically", root);
        status_->setWordWrap(true);
        layout->addWidget(status_);

        auto* buttons = new QHBoxLayout();
        start_ = new QPushButton("Start secure coordinator", root);
        stop_ = new QPushButton("Stop", root);
        stop_->setEnabled(false);
        phone_control_ = new QPushButton("Start Phone Control", root);
        phone_control_->setEnabled(false);
        buttons->addWidget(start_);
        buttons->addWidget(stop_);
        buttons->addWidget(phone_control_);
        layout->addLayout(buttons);

        auto* note = new QLabel(
            "WABridge uses TLS 1.3 and mutual certificate authentication. "
            "The default identity is stored with Windows DPAPI; PEM fields are optional test overrides. "
            "A first-pair connection remains pending until the device identities are compared.",
            root);
        note->setWordWrap(true);
        layout->addWidget(note);
        layout->addStretch(1);
        setCentralWidget(root);

        connect(start_, &QPushButton::clicked, this, &CoordinatorWindow::start_coordinator);
        connect(stop_, &QPushButton::clicked, this, &CoordinatorWindow::stop_coordinator);
        connect(phone_control_, &QPushButton::clicked, this, &CoordinatorWindow::toggle_phone_control);

        auto* timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, [this] {
            if (coordinator_ && coordinator_->running()) {
                status_->setText(QString("Listening on TCP port %1 — established sessions: %2")
                                     .arg(coordinator_->port())
                                     .arg(static_cast<qulonglong>(coordinator_->established_sessions())));
            }
        });
        timer->start(500);
    }

    ~CoordinatorWindow() override { stop_coordinator(); }

private:
    void set_status_async(QString text) {
        QMetaObject::invokeMethod(this, [this, text = std::move(text)] {
            status_->setText(text);
        }, Qt::QueuedConnection);
    }

    QWidget* with_browse(QLineEdit* field, const QString& title) {
        auto* container = new QWidget(this);
        auto* row = new QHBoxLayout(container);
        row->setContentsMargins(0, 0, 0, 0);
        auto* browse = new QPushButton("Browse…", container);
        row->addWidget(field, 1);
        row->addWidget(browse);
        connect(browse, &QPushButton::clicked, this, [this, field, title] {
            const QString path = QFileDialog::getOpenFileName(this, title, {}, "PEM files (*.pem *.crt *.key);;All files (*)");
            if (!path.isEmpty()) field->setText(path);
        });
        return container;
    }

    void start_coordinator() {
        try {
            std::string certificate;
            std::string private_key;
            std::string fingerprint;
            if (cert_path_->text().isEmpty() && key_path_->text().isEmpty()) {
                const auto material = wabridge::identity::Store().load_or_create();
                certificate = material.certificate_pem;
                private_key = material.private_key_pem;
                fingerprint = material.fingerprint;
            } else {
                if (cert_path_->text().isEmpty() || key_path_->text().isEmpty()) {
                    QMessageBox::warning(this, "WABridge", "Provide both PEM override paths, or leave both empty for the DPAPI identity.");
                    return;
                }
                certificate = read_file(cert_path_->text());
                private_key = read_file(key_path_->text());
                if (certificate.empty() || private_key.empty()) {
                    QMessageBox::warning(this, "WABridge", "The PEM override files are not readable.");
                    return;
                }
            }

            const QByteArray capability_bytes = QCryptographicHash::hash(
            QByteArrayLiteral("windows-coordinator-v1"), QCryptographicHash::Sha256);
        wabridge::messages::SessionHello hello;
        hello.role = wabridge::messages::Role::Windows;
        hello.session_nonce = wabridge::messages::fresh_session_nonce();
        hello.device_id = qEnvironmentVariable("COMPUTERNAME", "windows-coordinator").toStdString();
        if (hello.device_id.empty() || hello.device_id.size() > 64) hello.device_id = "windows-coordinator";
        std::copy(capability_bytes.cbegin(), capability_bytes.cend(), hello.capabilities_hash.begin());
        hello.max_frame = 4U * 1024U * 1024U;

            auto context = wabridge::tls::Context::server(certificate, private_key, std::nullopt);
            auto coordinator = std::make_shared<wabridge::coordinator::SecureCoordinator>(std::move(context), std::move(hello));
            wabridge::features::Dispatcher feature_dispatcher;
            feature_dispatcher.on_file_offer = [this](const wabridge::file::Offer& offer) {
                set_status_async(QString("Incoming file offer: %1 (%2 bytes)")
                                     .arg(QString::fromStdString(offer.display_name))
                                     .arg(static_cast<qulonglong>(offer.size)));
                return true;
            };
            feature_dispatcher.on_file_chunk = [this](const wabridge::file::Chunk& chunk) {
                set_status_async(QString("Incoming file chunk: offset %1, %2 bytes")
                                     .arg(static_cast<qulonglong>(chunk.offset))
                                     .arg(static_cast<qulonglong>(chunk.data.size())));
                return true;
            };
            feature_dispatcher.on_clipboard_update = [this](const wabridge::clipboard::Update& update) {
                set_status_async(QString("Clipboard update from %1 (%2 characters)")
                                     .arg(QString::fromStdString(update.origin_device_id))
                                     .arg(static_cast<qulonglong>(update.text.size())));
                return true;
            };
            feature_dispatcher.on_audio_frame = [this](const wabridge::audio::Frame& frame) {
                if (frame.codec != wabridge::audio::Codec::Pcm16) return false;
                std::lock_guard lock(audio_mutex_);
                if (!renderer_.running() && !renderer_.start(frame.sample_rate, frame.channels)) {
                    set_status_async("Unable to open the Windows default audio renderer");
                    return false;
                }
                if (!renderer_.render(frame)) {
                    renderer_.stop();
                    set_status_async("Rejected or failed to render the incoming PCM16 audio frame");
                    return false;
                }
                return true;
            };
            feature_dispatcher.on_input_event = [this](const wabridge::input::Event&) {
                set_status_async("Phone-control input event received");
                return true;
            };
            feature_dispatcher.on_display_command = [this](const wabridge::display::Command& command) {
                set_status_async(QString("Display mode command received: mode %1, sequence %2")
                                     .arg(static_cast<int>(command.mode))
                                     .arg(command.sequence));
                return true;
            };
            coordinator->set_feature_dispatcher(std::move(feature_dispatcher));
            if (!coordinator->start(static_cast<std::uint16_t>(port_->value()))) {
                status_->setText("Unable to bind the requested TCP port");
                return;
            }
            coordinator_ = std::move(coordinator);
            const auto weak_coordinator = std::weak_ptr<wabridge::coordinator::SecureCoordinator>(coordinator_);
            auto request_counter = std::make_shared<std::atomic<std::uint32_t>>(1);
            input_capture_ = std::make_unique<wabridge::platform_input::LowLevelCapture>(
                [weak_coordinator, request_counter](const wabridge::input::Event& event) {
                    try {
                        const auto peer = weak_coordinator.lock();
                        if (!peer) return;
                        const auto payload = wabridge::input::encode_event(event);
                        std::uint32_t request_id = request_counter->fetch_add(1);
                        if (request_id == 0) request_id = request_counter->fetch_add(1);
                        (void)peer->send({1, wabridge::features::kInputEvent, 0, request_id, payload});
                    } catch (const wabridge::protocol::ProtocolError&) {
                        // The bounded codec is fail-closed; malformed local events are dropped.
                    }
                });
            start_->setEnabled(false);
            stop_->setEnabled(true);
            phone_control_->setEnabled(true);
            status_->setText(QString("Listening on TCP port %1 — awaiting Android%s")
                                 .arg(coordinator_->port())
                                 .arg(fingerprint.empty() ? QString() : QString(" — identity %1").arg(QString::fromStdString(fingerprint))));
    } catch (const std::exception& error) {
            status_->setText(QString("Secure coordinator failed: %1").arg(error.what()));
            coordinator_.reset();
        }
    }

    void toggle_phone_control() {
        if (!input_capture_) return;
        if (input_capture_->active()) {
            input_capture_->stop();
            phone_control_->setText("Start Phone Control");
            status_->setText("Phone Control stopped");
            return;
        }
        if (!input_capture_->start()) {
            status_->setText("Phone Control could not start; Windows hook permission or platform support is unavailable");
            return;
        }
        phone_control_->setText("Stop Phone Control");
        status_->setText("Phone Control active — captured input is sent only over the authenticated session");
    }

    void stop_coordinator() {
        if (input_capture_) {
            input_capture_->stop();
            phone_control_->setText("Start Phone Control");
        }
        {
            std::lock_guard lock(audio_mutex_);
            renderer_.stop();
        }
        if (!coordinator_) return;
        coordinator_->stop();
        coordinator_.reset();
        input_capture_.reset();
        start_->setEnabled(true);
        stop_->setEnabled(false);
        phone_control_->setEnabled(false);
        status_->setText("Stopped");
    }

    QLineEdit* cert_path_{};
    QLineEdit* key_path_{};
    QSpinBox* port_{};
    QLabel* status_{};
    QPushButton* start_{};
    QPushButton* stop_{};
    QPushButton* phone_control_{};
    std::shared_ptr<wabridge::coordinator::SecureCoordinator> coordinator_;
    std::unique_ptr<wabridge::platform_input::LowLevelCapture> input_capture_;
    wabridge::platform_audio::WasapiRenderer renderer_;
    std::mutex audio_mutex_;
};

} // namespace

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    CoordinatorWindow window;
    window.show();
    return application.exec();
}
