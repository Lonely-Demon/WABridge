#include "wabridge_messages.h"
#include "wabridge_identity.h"
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
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <memory>
#include <optional>
#include <string>

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
        buttons->addWidget(start_);
        buttons->addWidget(stop_);
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
            auto coordinator = std::make_unique<wabridge::coordinator::SecureCoordinator>(std::move(context), std::move(hello));
            if (!coordinator->start(static_cast<std::uint16_t>(port_->value()))) {
                status_->setText("Unable to bind the requested TCP port");
                return;
            }
            coordinator_ = std::move(coordinator);
            start_->setEnabled(false);
            stop_->setEnabled(true);
            status_->setText(QString("Listening on TCP port %1 — awaiting Android%s")
                                 .arg(coordinator_->port())
                                 .arg(fingerprint.empty() ? QString() : QString(" — identity %1").arg(QString::fromStdString(fingerprint))));
    } catch (const std::exception& error) {
            status_->setText(QString("Secure coordinator failed: %1").arg(error.what()));
            coordinator_.reset();
        }
    }

    void stop_coordinator() {
        if (!coordinator_) return;
        coordinator_->stop();
        coordinator_.reset();
        start_->setEnabled(true);
        stop_->setEnabled(false);
        status_->setText("Stopped");
    }

    QLineEdit* cert_path_{};
    QLineEdit* key_path_{};
    QSpinBox* port_{};
    QLabel* status_{};
    QPushButton* start_{};
    QPushButton* stop_{};
    std::unique_ptr<wabridge::coordinator::SecureCoordinator> coordinator_;
};

} // namespace

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    CoordinatorWindow window;
    window.show();
    return application.exec();
}
