#pragma once

#include <microReticulum/Log.h> // I've commented out that annoying warning here...
#if !defined(RNS_LOG_LEVEL)
#define RNS_LOG_LEVEL RNS_LOG_LEVEL_VERBOSE
#endif
#if !defined(USTORE_USE_POSIXFS)
#define USTORE_USE_POSIXFS
#endif

#include <cstdint>
#include <string>
#include <functional>

#include <microReticulum.h>

namespace neroshop {

class Reticulum {
public:
    Reticulum();
    ~Reticulum();

    bool start();
    void stop();
    void loop();

    std::string create_identity();  // Creates (or restores) our identity keypair. Returns the identity hash hex.
    std::string create_identity(const std::string& keyfile_path);
    std::string create_destination(const char* app_name, const char* aspects); // Creates our IN destination (used to receive packets/announces) under
    // app_name/aspects, e.g. ("neroshop", "node"). Returns destination hash hex.

    std::string announce(const std::string& data); // Announces our destination on the network, with optional app_data attached.

    void send(const std::string& destination, const std::vector<uint8_t>& data); // Sends a raw packet to a peer we already know the destination hash for.

    bool add_tcp_client(const std::string& host, uint16_t port);

    void set_packet_handler(std::function<void(const std::vector<uint8_t>&)> cb);

    RNS::Reticulum reticulum;
    RNS::Interface udp_interface;
    RNS::Interface tcp_interface;
    RNS::Identity identity;
    RNS::Destination destination;
private:
    static void on_packet_static(const RNS::Bytes& data, const RNS::Packet& packet);
    void on_packet(const RNS::Bytes& data, const RNS::Packet& packet);

    std::function<void(const std::vector<uint8_t>&)> packet_callback;

    // RNS::Destination::set_packet_callback expects a plain function pointer
    // (see the onPacket()/onPingPacket() examples), not a std::function, so we
    // need a single static trampoline back into whichever instance registered last.
    static Reticulum* instance_;
};

}
