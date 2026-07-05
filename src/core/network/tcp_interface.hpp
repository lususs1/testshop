#pragma once
#include <microReticulum/Interface.h>
#include <microReticulum/Bytes.h>
#include <microReticulum/Type.h>

#include <string>
#include <vector>
#include <cstdint>

#ifndef DEFAULT_TCP_CLIENT_PORT
#define DEFAULT_TCP_CLIENT_PORT 4242
#endif
#ifndef TCP_CONNECT_TIMEOUT_MS
#define TCP_CONNECT_TIMEOUT_MS 5000
#endif
#ifndef TCP_RECONNECT_INTERVAL_MS
#define TCP_RECONNECT_INTERVAL_MS 5000
#endif
#ifndef TCP_KEEPALIVE_TIMEOUT_MS
#define TCP_KEEPALIVE_TIMEOUT_MS 300000
#endif

class TCPInterface : public RNS::InterfaceImpl {
public:
    static const uint32_t BITRATE_GUESS = 1000000;
    static constexpr uint8_t FRAME_START = 0x7E;
    static constexpr uint8_t FRAME_ESC   = 0x7D;
    static constexpr uint8_t FRAME_XOR   = 0x20;
    static constexpr size_t  RX_FRAME_MAX = 4096;

public:
    TCPInterface(const char* host, uint16_t port = DEFAULT_TCP_CLIENT_PORT, const char* name = "TCPInterface");
    virtual ~TCPInterface();

    virtual bool start();
    virtual void stop();
    virtual void loop();
    virtual inline std::string toString() const { return "TCPInterface[" + _name + "/" + _host + ":" + std::to_string(_port) + "]"; }

protected:
    virtual bool send_outgoing(const RNS::Bytes& data);
    void on_incoming(const RNS::Bytes& data);

private:
    enum ConnectState { CS_IDLE, CS_CONNECTING, CS_CONNECTED, CS_FAILED };

    bool beginConnect();
    bool pollConnect();   // returns true once resolved (success or failure)
    void sendFrame(const uint8_t* data, size_t len);
    int  readFrame();     // returns >0 frame len, 0 = incomplete, -1 = peer closed/error
    static long long now_ms();

    RNS::Bytes _buffer;

    std::string _host;
    uint16_t _port;

    int _socket = -1;
    ConnectState _connectState = CS_IDLE;

    long long _lastAttempt = 0;
    long long _lastRxTime = 0;

    bool _inFrame = false;
    bool _escaped = false;
    std::vector<uint8_t> _frameBuf;
    std::vector<uint8_t> _txScratch;
};
