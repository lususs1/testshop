#include "tcp_interface.hpp"

#include <microReticulum/Transport.h>
#include <microReticulum/Log.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <chrono>
#include <cstring>

using namespace RNS;

/*static*/ long long TCPInterface::now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

TCPInterface::TCPInterface(const char* host, uint16_t port, const char* name)
: RNS::InterfaceImpl(name), _host(host), _port(port)
{
    _IN = true;
    _OUT = true;
    _bitrate = BITRATE_GUESS;
    _HW_MTU = 500;
    _frameBuf.reserve(RX_FRAME_MAX);
    _txScratch.reserve(RX_FRAME_MAX);
}

/*virtual*/ TCPInterface::~TCPInterface() {
    stop();
}

/*virtual*/ bool TCPInterface::start() {
    _online = true;
    beginConnect();
    return true;
}

/*virtual*/ void TCPInterface::stop() {
    if (_socket > -1) {
        close(_socket);
        _socket = -1;
    }
    _connectState = CS_IDLE;
    _online = false;
}

bool TCPInterface::beginConnect() {
    _lastAttempt = now_ms();
    _connectState = CS_CONNECTING;

    INFOF("TCPInterface: connecting to %s:%d...", _host.c_str(), _port);

    struct addrinfo hints{};
    struct addrinfo* res = nullptr;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    char port_str[8];
    snprintf(port_str, sizeof(port_str), "%d", _port);

    if (getaddrinfo(_host.c_str(), port_str, &hints, &res) != 0 || res == nullptr) {
        ERRORF("TCPInterface: unable to resolve host %s", _host.c_str());
        _connectState = CS_FAILED;
        return false;
    }

    _socket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (_socket < 0) {
        ERRORF("TCPInterface: unable to create socket with error %d", errno);
        freeaddrinfo(res);
        _connectState = CS_FAILED;
        return false;
    }

    int flags = fcntl(_socket, F_GETFL, 0);
    fcntl(_socket, F_SETFL, flags | O_NONBLOCK);

    int rc = connect(_socket, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);

    if (rc == 0) {
        // Connected immediately (e.g. localhost)
        int one = 1;
        setsockopt(_socket, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
        _connectState = CS_CONNECTED;
    }
    else if (errno != EINPROGRESS) {
        ERRORF("TCPInterface: connect() failed with error %d", errno);
        close(_socket);
        _socket = -1;
        _connectState = CS_FAILED;
        return false;
    }
    // else: in progress, resolved later via pollConnect() from loop()

    return true;
}

bool TCPInterface::pollConnect() {
    if (now_ms() - _lastAttempt >= TCP_CONNECT_TIMEOUT_MS) {
        ERRORF("TCPInterface: connect() to %s:%d timed out", _host.c_str(), _port);
        close(_socket);
        _socket = -1;
        _connectState = CS_FAILED;
        return true;
    }

    fd_set wfds;
    FD_ZERO(&wfds);
    FD_SET(_socket, &wfds);
    timeval tv{0, 0}; // non-blocking check

    int ready = select(_socket + 1, nullptr, &wfds, nullptr, &tv);
    if (ready <= 0) {
        return false; // still connecting
    }

    int so_error = 0;
    socklen_t len = sizeof(so_error);
    getsockopt(_socket, SOL_SOCKET, SO_ERROR, &so_error, &len);
    if (so_error != 0) {
        ERRORF("TCPInterface: connect() to %s:%d failed with error %d", _host.c_str(), _port, so_error);
        close(_socket);
        _socket = -1;
        _connectState = CS_FAILED;
        return true;
    }

    int one = 1;
    setsockopt(_socket, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    _connectState = CS_CONNECTED;
    return true;
}

/*virtual*/ void TCPInterface::loop() {
    if (!_online) {
        return;
    }

    if (_connectState == CS_CONNECTING) {
        if (!pollConnect()) {
            return; // still waiting
        }
    }

    if (_connectState == CS_CONNECTED && _socket < 0) {
        // shouldn't happen, but guard anyway
        _connectState = CS_FAILED;
    }

    if (_connectState == CS_FAILED) {
        _connectState = CS_IDLE;
    }

    if (_connectState == CS_IDLE) {
        if (now_ms() - _lastAttempt >= TCP_RECONNECT_INTERVAL_MS) {
            beginConnect();
        }
        return;
    }

    // CS_CONNECTED from here on
    if (_lastRxTime > 0 && (now_ms() - _lastRxTime) >= TCP_KEEPALIVE_TIMEOUT_MS) {
        WARNINGF("TCPInterface: no RX for %llds, forcing reconnect to %s:%d",
                 (now_ms() - _lastRxTime) / 1000, _host.c_str(), _port);
        close(_socket);
        _socket = -1;
        _inFrame = false;
        _escaped = false;
        _frameBuf.clear();
        _connectState = CS_IDLE;
        _lastAttempt = now_ms();
        return;
    }

    // Drain complete frames, same "loop until empty" pattern as UDPInterface::loop()
    while (true) {
        int len = readFrame();
        if (len > 0) {
            _lastRxTime = now_ms();
            _buffer.assign(_frameBuf.data(), static_cast<size_t>(len));
            on_incoming(_buffer);
            continue;
        }
        if (len < 0) {
            WARNINGF("TCPInterface: connection to %s:%d closed", _host.c_str(), _port);
            close(_socket);
            _socket = -1;
            _inFrame = false;
            _escaped = false;
            _frameBuf.clear();
            _connectState = CS_IDLE;
            _lastAttempt = now_ms();
        }
        break; // len == 0: no more complete frames right now
    }
}

/*virtual*/ bool TCPInterface::send_outgoing(const Bytes& data) {
    DEBUGF("%s.on_outgoing: data: %s", toString().c_str(), data.toHex().c_str());
    bool success = true;
    try {
        if (_online && _connectState == CS_CONNECTED && _socket > -1) {
            sendFrame(data.data(), data.size());
        }
        else {
            DEBUGF("%s: TX skipped, interface not connected", toString().c_str());
            success = false;
        }

        InterfaceImpl::handle_outgoing(data);
    }
    catch (const std::exception& e) {
        ERRORF("Could not transmit on %s. The contained exception was: %s", toString().c_str(), e.what());
        success = false;
    }
    return success;
}

void TCPInterface::on_incoming(const Bytes& data) {
    DEBUGF("%s.on_incoming: data: %s", toString().c_str(), data.toHex().c_str());
    InterfaceImpl::handle_incoming(data);
}

// HDLC-like framing: [0x7E] [escaped data] [0x7E]
void TCPInterface::sendFrame(const uint8_t* data, size_t len) {
    _txScratch.clear();
    _txScratch.push_back(FRAME_START);
    for (size_t i = 0; i < len; i++) {
        if (data[i] == FRAME_START || data[i] == FRAME_ESC) {
            _txScratch.push_back(FRAME_ESC);
            _txScratch.push_back(data[i] ^ FRAME_XOR);
        }
        else {
            _txScratch.push_back(data[i]);
        }
    }
    _txScratch.push_back(FRAME_START);

    TRACEF("Sending TCP frame to %s:%d (%d bytes)", _host.c_str(), _port, (int)_txScratch.size());
    ssize_t sent = send(_socket, _txScratch.data(), _txScratch.size(), MSG_NOSIGNAL);
    if (sent != static_cast<ssize_t>(_txScratch.size())) {
        WARNINGF("Failed sending frame to %s:%d", _host.c_str(), _port);
    }
}

int TCPInterface::readFrame() {
    uint8_t tmp[1024];
    ssize_t n = recv(_socket, tmp, sizeof(tmp), MSG_DONTWAIT);
    if (n == 0) return -1;
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
        return -1;
    }

    for (ssize_t i = 0; i < n; i++) {
        uint8_t b = tmp[i];
        if (b == FRAME_START) {
            if (_inFrame && !_frameBuf.empty()) {
                return static_cast<int>(_frameBuf.size()); // caller reads _frameBuf, then clears
            }
            _inFrame = true;
            _frameBuf.clear();
            _escaped = false;
            continue;
        }
        if (!_inFrame) continue;
        if (b == FRAME_ESC) { _escaped = true; continue; }

        if (_frameBuf.size() >= RX_FRAME_MAX) {
            WARNING("TCPInterface: frame too large, dropping");
            _inFrame = false;
            _escaped = false;
            _frameBuf.clear();
            continue;
        }
        _frameBuf.push_back(_escaped ? (b ^ FRAME_XOR) : b);
        _escaped = false;
    }
    return 0;
}
