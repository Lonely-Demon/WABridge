#include "wabridge_messages.h"
#include "wabridge_feature_dispatch.h"
#include "wabridge_identity.h"
#include "wabridge_input.h"
#include "wabridge_input_capture.h"
#include "wabridge_wasapi_audio.h"
#include "wabridge_secure_coordinator.h"
#include "wabridge_tls.h"

#include <QApplication>
#include <QtNetwork/QAbstractSocket>
#include <QCryptographicHash>
#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMetaObject>
#include <QDialog>
#include <QPlainTextEdit>
#include <QtNetwork/QHostAddress>
#include <QtNetwork/QNetworkInterface>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QStackedWidget>
#include <QSpinBox>
#include <QStringList>
#include <QTimer>
#include <QtNetwork/QUdpSocket>
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
class MdnsAdvertiser final : public QObject {
public:
    explicit MdnsAdvertiser(QObject* parent = nullptr) : QObject(parent), timer_(this) {
        connect(&timer_, &QTimer::timeout, this, [this] { announce(); });
    }

    bool start(const QString& device_id, const quint16 port) {
        stop();
        if (device_id.trimmed().isEmpty() || port == 0) return false;
        device_id_ = device_id.trimmed().left(48);
        instance_ = QString("%1 WABridge").arg(device_id_).left(62);
        hostname_ = QString("wabridge-%1.local").arg(device_id_.replace(QRegularExpression("[^A-Za-z0-9-]"), "-").toLower()).left(62);
        port_ = port;
        socket_ = std::make_unique<QUdpSocket>();
        if (!socket_->bind(QHostAddress::AnyIPv4, 5353, QAbstractSocket::ShareAddress | QAbstractSocket::ReuseAddressHint)) {
            socket_.reset();
            return false;
        }
        connect(socket_.get(), &QUdpSocket::readyRead, this, [this] {
            while (socket_ && socket_->hasPendingDatagrams()) {
                QByteArray datagram;
                datagram.resize(static_cast<int>(socket_->pendingDatagramSize()));
                QHostAddress sender;
                quint16 sender_port = 0;
                socket_->readDatagram(datagram.data(), datagram.size(), &sender, &sender_port);
                announce(sender, sender_port);
            }
        });
        announce();
        timer_.start(3000);
        return true;
    }

    void stop() noexcept {
        timer_.stop();
        if (socket_) {
            socket_->close();
            socket_.reset();
        }
        device_id_.clear();
        instance_.clear();
        hostname_.clear();
        port_ = 0;
    }

private:
    static void append_u16(QByteArray& packet, const quint16 value) {
        packet.append(static_cast<char>((value >> 8) & 0xff));
        packet.append(static_cast<char>(value & 0xff));
    }

    static void append_u32(QByteArray& packet, const quint32 value) {
        packet.append(static_cast<char>((value >> 24) & 0xff));
        packet.append(static_cast<char>((value >> 16) & 0xff));
        packet.append(static_cast<char>((value >> 8) & 0xff));
        packet.append(static_cast<char>(value & 0xff));
    }

    static QByteArray dns_name(const QString& name) {
        QByteArray encoded;
        for (const auto& label : name.split('.')) {
            const auto bytes = label.toUtf8().left(63);
            encoded.append(static_cast<char>(bytes.size()));
            encoded.append(bytes);
        }
        encoded.append('\0');
        return encoded;
    }

    static void add_record(QByteArray& packet, const QString& owner, const quint16 type, const quint32 ttl, const QByteArray& data) {
        packet.append(dns_name(owner));
        append_u16(packet, type);
        append_u16(packet, 0x0001); // IN; mDNS cache-flush is not needed for these records.
        append_u32(packet, ttl);
        append_u16(packet, static_cast<quint16>(data.size()));
        packet.append(data);
    }

    void announce(const QHostAddress& target = QHostAddress("224.0.0.251"), const quint16 target_port = 5353) {
        if (!socket_ || instance_.isEmpty() || hostname_.isEmpty()) return;
        const QString service = QString("%1._wabridge._tcp.local").arg(instance_);
        const QString service_type = "_wabridge._tcp.local";
        const auto addresses = QNetworkInterface::allAddresses();
        quint16 answer_count = 3;
        for (const auto& address : addresses) {
            if (address.protocol() == QAbstractSocket::IPv4Protocol && !address.isLoopback()) ++answer_count;
        }
        QByteArray packet;
        append_u16(packet, 0);
        append_u16(packet, 0x8400); // unsolicited authoritative response
        append_u16(packet, answer_count);
        append_u16(packet, 0);
        append_u16(packet, 0);
        add_record(packet, service_type, 12, 120, dns_name(service));

        QByteArray srv;
        append_u16(srv, 0);
        append_u16(srv, 0);
        append_u16(srv, port_);
        srv.append(dns_name(hostname_));
        add_record(packet, service, 33, 120, srv);

        QByteArray txt;
        const auto add_txt = [&txt](const QByteArray& value) {
            const auto bounded = value.left(255);
            txt.append(static_cast<char>(bounded.size()));
            txt.append(bounded);
        };
        add_txt("version=1");
        add_txt("role=windows");
        add_txt(QString("device_id=%1").arg(device_id_).toUtf8());
        add_record(packet, service, 16, 120, txt);

        for (const auto& address : addresses) {
            if (address.protocol() != QAbstractSocket::IPv4Protocol || address.isLoopback()) continue;
            const quint32 ip = address.toIPv4Address();
            QByteArray a_record;
            a_record.append(static_cast<char>((ip >> 24) & 0xff));
            a_record.append(static_cast<char>((ip >> 16) & 0xff));
            a_record.append(static_cast<char>((ip >> 8) & 0xff));
            a_record.append(static_cast<char>(ip & 0xff));
            add_record(packet, hostname_, 1, 120, a_record);
        }
        socket_->writeDatagram(packet, target, target_port);
    }

    QTimer timer_;
    std::unique_ptr<QUdpSocket> socket_;
    QString device_id_;
    QString instance_;
    QString hostname_;
    quint16 port_{0};
};

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
        resize(1080, 720);
        setMinimumSize(900, 620);
        setStyleSheet(R"(
            QMainWindow, QWidget { background: #111317; color: #eef2f7; font-family: "Segoe UI"; }
            QLabel { background: transparent; }
            QFrame#rail { background: #171a20; border-right: 1px solid #2b313b; }
            QLabel#brand { color: #f5f7fb; font-size: 24px; font-weight: 700; }
            QLabel#eyebrow { color: #8390a3; font-size: 10px; font-weight: 700; letter-spacing: 1px; }
            QLabel#muted { color: #9aa6b6; }
            QLabel#title { color: #f7f9fc; font-size: 28px; font-weight: 700; }
            QLabel#subtitle { color: #9aa6b6; font-size: 13px; }
            QLabel#statusPill { background: #193a2a; color: #55e68b; border: 1px solid #28633e; border-radius: 12px; padding: 6px 12px; font-weight: 700; }
            QFrame#card { background: #1b1f27; border: 1px solid #2c3440; border-radius: 16px; }
            QFrame#hero { background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #1d2d51, stop:1 #182028); border: 1px solid #31588d; border-radius: 18px; }
            QLabel#heroIcon { background: #243e71; color: #72a6ff; border-radius: 16px; padding: 18px; font-size: 30px; }
            QLabel#cardTitle { color: #f1f4f8; font-size: 15px; font-weight: 700; }
            QLabel#cardText { color: #9aa6b6; font-size: 12px; }
            QPushButton { border-radius: 9px; padding: 10px 16px; font-weight: 600; }
            QPushButton#primary { background: #1677e8; color: white; border: 1px solid #3e9aff; }
            QPushButton#primary:hover { background: #2588f5; }
            QPushButton#secondary { background: #202630; color: #dbe5f1; border: 1px solid #3a4655; }
            QPushButton#secondary:hover { background: #293340; }
            QPushButton#nav { background: transparent; color: #9aa6b6; text-align: left; border-radius: 9px; padding: 11px 14px; }
            QPushButton#nav:hover, QPushButton#nav[selected="true"] { background: #22304a; color: #72a6ff; }
            QGroupBox { color: #9aa6b6; border: 1px solid #2c3440; border-radius: 12px; margin-top: 12px; padding: 12px; }
            QGroupBox::title { subcontrol-origin: margin; left: 14px; padding: 0 5px; }
            QLineEdit, QSpinBox { background: #14171c; color: #eef2f7; border: 1px solid #343d4a; border-radius: 7px; padding: 8px; }
            QLineEdit:focus, QSpinBox:focus { border: 1px solid #4b8eff; }
        )");

        auto* root = new QWidget(this);
        auto* shell = new QHBoxLayout(root);
        shell->setContentsMargins(0, 0, 0, 0);
        shell->setSpacing(0);

        auto* rail = new QFrame(root);
        rail->setObjectName("rail");
        rail->setFixedWidth(220);
        auto* railLayout = new QVBoxLayout(rail);
        railLayout->setContentsMargins(22, 28, 18, 24);
        railLayout->setSpacing(8);
        auto* brand = new QLabel("WABridge", rail);
        brand->setObjectName("brand");
        railLayout->addWidget(brand);
        auto* brandSub = new QLabel("WINDOWS COORDINATOR", rail);
        brandSub->setObjectName("eyebrow");
        railLayout->addWidget(brandSub);
        railLayout->addSpacing(28);
        for (const auto& label : {QString("⌂  Dashboard"), QString("◉  Phone Control"), QString("♫  Audio"), QString("□  Files"), QString("▣  Clipboard"), QString("▤  Logs"), QString("⚙  Settings")}) {
            auto* nav = new QPushButton(label, rail);
            nav->setObjectName("nav");
            nav->setProperty("selected", label.startsWith("⌂"));
            nav->setCursor(Qt::PointingHandCursor);
            connect(nav, &QPushButton::clicked, this, [this, label] {
                if (label.contains("Logs")) {
                    show_logs();
                    return;
                }
                status_->setText(label.contains("Dashboard")
                    ? "Ready to connect — start the secure Windows coordinator"
                    : QString("%1 is ready for the authenticated Android session").arg(label.trimmed()));
            });
            railLayout->addWidget(nav);
        }
        railLayout->addStretch(1);
        auto* secure = new QLabel("TLS 1.3\nCertificate pinning\nWi-Fi only", rail);
        secure->setObjectName("muted");
        secure->setWordWrap(true);
        railLayout->addWidget(secure);
        shell->addWidget(rail);

        auto* content = new QWidget(root);
        auto* contentLayout = new QVBoxLayout(content);
        contentLayout->setContentsMargins(34, 26, 34, 26);
        contentLayout->setSpacing(18);
        auto* top = new QHBoxLayout();
        auto* headingBox = new QVBoxLayout();
        auto* eyebrow = new QLabel("LOCAL WORKSPACE BRIDGE", content);
        eyebrow->setObjectName("eyebrow");
        headingBox->addWidget(eyebrow);
        auto* title = new QLabel("Your devices, working together", content);
        title->setObjectName("title");
        headingBox->addWidget(title);
        auto* subtitle = new QLabel("Connect Android to Windows over your trusted Wi-Fi network.", content);
        subtitle->setObjectName("subtitle");
        headingBox->addWidget(subtitle);
        top->addLayout(headingBox, 1);
        auto* pill = new QLabel("●  OFFLINE", content);
        pill->setObjectName("statusPill");
        pill->setAlignment(Qt::AlignCenter);
        top->addWidget(pill, 0, Qt::AlignTop);
        contentLayout->addLayout(top);

        auto* hero = new QFrame(content);
        hero->setObjectName("hero");
        auto* heroLayout = new QHBoxLayout(hero);
        heroLayout->setContentsMargins(22, 20, 22, 20);
        auto* heroIcon = new QLabel("◫", hero);
        heroIcon->setObjectName("heroIcon");
        heroIcon->setAlignment(Qt::AlignCenter);
        heroLayout->addWidget(heroIcon, 0, Qt::AlignTop);
        auto* heroCopy = new QVBoxLayout();
        auto* heroTitle = new QLabel("Connect your Android phone", hero);
        heroTitle->setObjectName("cardTitle");
        heroCopy->addWidget(heroTitle);
        auto* heroText = new QLabel("Discover a nearby device or use the manual IP fallback. First pairing always requires fingerprint approval.", hero);
        heroText->setObjectName("cardText");
        heroText->setWordWrap(true);
        heroCopy->addWidget(heroText);
        heroCopy->addSpacing(12);
        auto* buttons = new QHBoxLayout();
        start_ = new QPushButton("Start secure coordinator", hero);
        start_->setObjectName("primary");
        stop_ = new QPushButton("Stop", hero);
        stop_->setObjectName("secondary");
        stop_->setEnabled(false);
        phone_control_ = new QPushButton("Start Phone Control", hero);
        phone_control_->setObjectName("secondary");
        phone_control_->setEnabled(false);
        buttons->addWidget(start_);
        buttons->addWidget(stop_);
        buttons->addWidget(phone_control_);
        heroCopy->addLayout(buttons);
        heroLayout->addLayout(heroCopy, 1);
        contentLayout->addWidget(hero);

        auto* capabilities = new QGridLayout();
        capabilities->setHorizontalSpacing(12);
        capabilities->setVerticalSpacing(12);
        const auto addCapability = [content](QGridLayout* grid, int row, int column, const QString& icon, const QString& name, const QString& detail, const QString& state, const QString& color) {
            auto* card = new QFrame(content);
            card->setObjectName("card");
            auto* cardLayout = new QHBoxLayout(card);
            cardLayout->setContentsMargins(16, 14, 16, 14);
            auto* glyph = new QLabel(icon, card);
            glyph->setStyleSheet(QString("color: %1; font-size: 22px; background: %1 22; border-radius: 10px; padding: 8px;").arg(color));
            glyph->setAlignment(Qt::AlignCenter);
            cardLayout->addWidget(glyph);
            auto* copy = new QVBoxLayout();
            auto* nameLabel = new QLabel(name, card);
            nameLabel->setObjectName("cardTitle");
            copy->addWidget(nameLabel);
            auto* detailLabel = new QLabel(detail, card);
            detailLabel->setObjectName("cardText");
            copy->addWidget(detailLabel);
            auto* stateLabel = new QLabel(state, card);
            stateLabel->setStyleSheet(QString("color: %1; font-size: 11px; font-weight: 700;").arg(color));
            copy->addWidget(stateLabel);
            cardLayout->addLayout(copy, 1);
            grid->addWidget(card, row, column);
        };
        addCapability(capabilities, 0, 0, "◫", "Second Display", "Android as an extra Windows screen", "READY AFTER SESSION", "#72a6ff");
        addCapability(capabilities, 0, 1, "◉", "Phone Control", "Mouse and keyboard to Android", "USER AUTHORIZATION REQUIRED", "#5de6ff");
        addCapability(capabilities, 1, 0, "♫", "Android Audio", "Play phone audio on Windows", "MEDIA PROJECTION REQUIRED", "#32d74b");
        addCapability(capabilities, 1, 1, "□", "Files + Clipboard", "Move content between devices", "READY AFTER SESSION", "#ffd60a");
        contentLayout->addLayout(capabilities);

        auto* advanced = new QGroupBox("Advanced security and network settings", content);
        auto* form = new QFormLayout(advanced);
        cert_path_ = new QLineEdit(advanced);
        key_path_ = new QLineEdit(advanced);
        cert_path_->setPlaceholderText("Optional PEM certificate override");
        key_path_->setPlaceholderText("Optional PEM private-key override");
        form->addRow("Device certificate:", with_browse(cert_path_, "Select certificate"));
        form->addRow("Private key:", with_browse(key_path_, "Select private key"));
        port_ = new QSpinBox(advanced);
        port_->setRange(1, 65535);
        port_->setValue(51820);
        form->addRow("TCP port:", port_);
        contentLayout->addWidget(advanced);

        auto* statusCard = new QFrame(content);
        statusCard->setObjectName("card");
        auto* statusLayout = new QVBoxLayout(statusCard);
        status_ = new QLabel("Ready to connect — a DPAPI-protected Windows identity will be created automatically", statusCard);
        status_->setObjectName("muted");
        status_->setWordWrap(true);
        statusLayout->addWidget(status_);
        contentLayout->addWidget(statusCard);
        contentLayout->addStretch(1);
        shell->addWidget(content, 1);
        setCentralWidget(root);
        mdns_ = std::make_unique<MdnsAdvertiser>(this);

        connect(start_, &QPushButton::clicked, this, &CoordinatorWindow::start_coordinator);
        connect(stop_, &QPushButton::clicked, this, &CoordinatorWindow::stop_coordinator);
        connect(phone_control_, &QPushButton::clicked, this, &CoordinatorWindow::toggle_phone_control);

        auto* timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, [this, pill] {
            if (coordinator_ && coordinator_->running()) {
                status_->setText(QString("Listening on TCP port %1 — established sessions: %2")
                                     .arg(coordinator_->port())
                                     .arg(static_cast<qulonglong>(coordinator_->established_sessions())));
                pill->setText("●  LISTENING");
                pill->setStyleSheet("background: #193a2a; color: #55e68b; border: 1px solid #28633e; border-radius: 12px; padding: 6px 12px; font-weight: 700;");
            }
        });
        timer->start(500);
        QTimer::singleShot(0, this, &CoordinatorWindow::start_coordinator);
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

    void append_log(const QString& message) {
        const auto line = QString("[%1] %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss"), message.trimmed());
        log_entries_.append(line);
        while (log_entries_.size() > 300) log_entries_.removeFirst();
    }

    void show_logs() {
        QDialog dialog(this);
        dialog.setWindowTitle("WABridge — Connection logs");
        dialog.resize(760, 520);
        auto* layout = new QVBoxLayout(&dialog);
        auto* heading = new QLabel("Connection logs", &dialog);
        heading->setObjectName("title");
        layout->addWidget(heading);
        auto* description = new QLabel("Listener, Wi-Fi advertisement, TCP, TLS, pairing, and feature events. Private keys are never logged.", &dialog);
        description->setObjectName("subtitle");
        description->setWordWrap(true);
        layout->addWidget(description);
        auto* text = new QPlainTextEdit(&dialog);
        text->setReadOnly(true);
        text->setStyleSheet("QPlainTextEdit { background: #0d1014; color: #d9e7f4; border: 1px solid #2c3440; border-radius: 10px; padding: 10px; font-family: Consolas; }");
        text->setPlainText(log_entries_.join("\n"));
        layout->addWidget(text, 1);
        auto* buttons = new QHBoxLayout();
        auto* clear = new QPushButton("Clear logs", &dialog);
        auto* close = new QPushButton("Close", &dialog);
        buttons->addWidget(clear);
        buttons->addStretch(1);
        buttons->addWidget(close);
        layout->addLayout(buttons);
        connect(clear, &QPushButton::clicked, this, [this, text] {
            log_entries_.clear();
            text->clear();
        });
        connect(close, &QPushButton::clicked, &dialog, &QDialog::accept);
        dialog.exec();
    }

    void start_coordinator() {
        append_log("Starting Windows coordinator");
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
            const QString advertised_device_id = QString::fromStdString(hello.device_id);
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
                append_log(QString("TCP listener failed to bind port %1").arg(port_->value()));
                status_->setText("Unable to bind the requested TCP port");
                return;
            }
            append_log(QString("TCP listener bound to port %1").arg(coordinator->port()));
            coordinator_ = std::move(coordinator);
            const bool mdns_ready = mdns_ && mdns_->start(advertised_device_id, coordinator_->port());
            append_log(mdns_ready ? "mDNS availability advertisement started" : "mDNS advertisement unavailable; manual IP and firewall checks required");
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
            append_log(QString("Coordinator ready on TCP port %1").arg(coordinator_->port()));
            status_->setText(QString("Listening on TCP port %1 — awaiting Android%s%2")
                                 .arg(coordinator_->port())
                                 .arg(fingerprint.empty() ? QString() : QString(" — identity %1").arg(QString::fromStdString(fingerprint)))
                                 .arg(mdns_ready ? " — mDNS advertised" : " — mDNS unavailable; use manual IP"));
    } catch (const std::exception& error) {
            append_log(QString("Secure coordinator failed: %1").arg(error.what()));
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
        append_log("Stopping Windows coordinator");
        if (input_capture_) {
            input_capture_->stop();
            phone_control_->setText("Start Phone Control");
        }
        {
            std::lock_guard lock(audio_mutex_);
            renderer_.stop();
        }
        if (mdns_) mdns_->stop();
        if (!coordinator_) return;
        coordinator_->stop();
        coordinator_.reset();
        input_capture_.reset();
        start_->setEnabled(true);
        stop_->setEnabled(false);
        phone_control_->setEnabled(false);
        status_->setText("Stopped");
        append_log("Windows coordinator stopped");
    }

    QLineEdit* cert_path_{};
    QLineEdit* key_path_{};
    QSpinBox* port_{};
    QLabel* status_{};
    QPushButton* start_{};
    QPushButton* stop_{};
    QPushButton* phone_control_{};
    std::unique_ptr<MdnsAdvertiser> mdns_;
    std::shared_ptr<wabridge::coordinator::SecureCoordinator> coordinator_;
    std::unique_ptr<wabridge::platform_input::LowLevelCapture> input_capture_;
    wabridge::platform_audio::WasapiRenderer renderer_;
    QStringList log_entries_;
    std::mutex audio_mutex_;
};

} // namespace

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    CoordinatorWindow window;
    window.show();
    return application.exec();
}
