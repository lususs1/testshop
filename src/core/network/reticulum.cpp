#include "reticulum.hpp"

#include "../../neroshop_config.hpp" // get_default_config_path()

// microReticulum headers
#include <microStore/FileSystem.h>
#include <microStore/Adapters/PosixFileSystem.h>
#include "udp_interface.hpp"
#include "tcp_interface.hpp"

#include <microReticulum.h>

namespace neroshop {
//namespace net {

//-----------------------------------------------------------------------------

Reticulum::Reticulum() : reticulum({RNS::Type::NONE}), udp_interface({RNS::Type::NONE}),
    tcp_interface({RNS::Type::NONE}), identity({RNS::Type::NONE}), destination({RNS::Type::NONE}) {}

//-----------------------------------------------------------------------------

Reticulum::~Reticulum() { stop(); }

//-----------------------------------------------------------------------------

Reticulum* Reticulum::instance_ = nullptr;

//-----------------------------------------------------------------------------

bool Reticulum::start() {
    INFO("Setting up Reticulum...");
    instance_ = this;

    // from udp_transport example:
    try {

        // Initialize and register filesystem
        INFO("Registering FileSystem with OS...");
        microStore::FileSystem filesystem{microStore::Adapters::PosixFileSystem()};
        filesystem.init();
        RNS::Utilities::OS::register_filesystem(filesystem);

        // Initialize and register interface
        INFO("Registering UDPInterface instances with Transport...");
        udp_interface = new UDPInterface();
        udp_interface.mode(RNS::Type::Interface::MODE_GATEWAY);
        RNS::Transport::register_interface(udp_interface);
        udp_interface.start();

        INFO("Creating Reticulum instance...");
        reticulum = RNS::Reticulum();
        reticulum.transport_enabled(false);
        reticulum.probe_destination_enabled(true);
        //reticulum.remote_management_enabled(true); // <- uncomment if transport=true
        reticulum.start();

        std::string keyfile = neroshop::get_default_config_path() + "/reticulum_identity.key";
        // Set up identity + destination + inbound callback right away so
        // Node can immediately announce()/send() after start() returns.
        create_identity(keyfile);
        create_destination("neroshop", "node");

        INFO("RNS Transport Ready!");
        return true;
    }
    catch (const std::exception& e) {
        ERRORF("!!! Exception in Reticulum::start: %s", e.what());
        return false;
    }

    // from udp_announce example:
	/*try {

		//std::stringstream test;
		// !!! just adding this single stringstream alone (not even using it) adds a whopping 17.1% !!!
		// !!! JUST SAY NO TO STRINGSTREAM !!!

		// 18.5% completely empty program

		// 21.8% baseline here with serial

		// Initialize and register filesystem
		HEAD("Registering FileSystem with OS...", RNS::LOG_TRACE);
		microStore::FileSystem filesystem{microStore::Adapters::UniversalFileSystem()};
		filesystem.init();
		RNS::Utilities::OS::register_filesystem(filesystem);

		// Initialize and register interface
		HEAD("Registering UDPInterface instances with Transport...", RNS::LOG_TRACE);
		udp_interface = new UDPInterface();
		udp_interface.mode(RNS::Type::Interface::MODE_GATEWAY);
		RNS::Transport::register_interface(udp_interface);
		udp_interface.start();

		HEAD("Creating Reticulum instance...", RNS::LOG_TRACE);
		//RNS::Reticulum reticulum;
		reticulum = RNS::Reticulum({RNS::Type::NONE});
		reticulum.transport_enabled(false);
		reticulum.probe_destination_enabled(true);
		reticulum.start();
		//return;
		// 21.9% (+0.1%)

		HEAD("Creating Identity instance...", RNS::LOG_TRACE);
		// new identity
		//RNS::Identity identity;
		//identity = RNS::Identity();
		// predefined identity
		//RNS::Identity identity(false);
		identity = RNS::Identity(false);
		RNS::Bytes prv_bytes;

        prv_bytes.assignHex("E0D43398EDC974EBA9F4A83463691A08F4D306D4E56BA6B275B8690A2FBD9852E9EBE7C03BC45CAEC9EF8E78C830037210BFB9986F6CA2DEE2B5C28D7B4DE6B0");

        identity.load_private_key(prv_bytes);
        // 22.6% (+0.7%)

        HEAD("Creating Destination instance...", RNS::LOG_TRACE);
        //RNS::Destination destination(identity, RNS::Type::Destination::IN, RNS::Type::Destination::SINGLE, "app", "aspects");
        destination = RNS::Destination(identity, RNS::Type::Destination::IN, RNS::Type::Destination::SINGLE, "app", "aspects");
        // 23.0% (+0.4%)

        // Register DATA packet callback
        HEAD("Registering packet callback with Destination...", RNS::LOG_TRACE);
        destination.set_packet_callback(onPacket);
        destination.set_proof_strategy(RNS::Type::Destination::PROVE_ALL);

        {
            // Register PING packet callback
            HEAD("Creating PING Destination instance...", RNS::LOG_TRACE);
            RNS::Destination ping_destination(identity, RNS::Type::Destination::IN, RNS::Type::Destination::SINGLE, "example_utilities", "echo.request");

            HEAD("Registering packet callback with PING Destination...", RNS::LOG_TRACE);
            ping_destination.set_packet_callback(onPingPacket);
            ping_destination.set_proof_strategy(RNS::Type::Destination::PROVE_ALL);
        }

        HEAD("Registering announce handler with Transport...", RNS::LOG_TRACE);
        RNS::Transport::register_announce_handler(announce_handler);*/

        /*
         * HEAD("Announcing destination...", RNS::LOG_TRACE);
         * //destination.announce(RNS::bytesFromString(fruits[RNS::Cryptography::randomnum() % 7]));
         * // test path
         * //destination.announce(RNS::bytesFromString(fruits[RNS::Cryptography::randomnum() % 7]), true, nullptr, RNS::bytesFromString("test_tag"));
         * // test packet send
         * destination.announce(RNS::bytesFromString(fruits[RNS::Cryptography::randomnum() % 7]));
         * // 23.9% (+0.8%)
         */

        /*#if defined (RETICULUM_PACKET_TEST)
        // test data send packet
        HEAD("Creating send packet...", RNS::LOG_TRACE);
        RNS::Packet send_packet(destination, "The quick brown fox jumps over the lazy dog");

        HEAD("Sending send packet...", RNS::LOG_TRACE);
        send_packet.pack();
        #ifndef NDEBUG
        TRACEF("Test send_packet: %s", send_packet.debugString().c_str());
        #endif

        HEAD("Creating recv packet...", RNS::LOG_TRACE);
        RNS::Packet recv_packet({RNS::Type::NONE}, send_packet.raw());
        recv_packet.unpack();
        #ifndef NDEBUG
        TRACEF("Test recv_packet: %s", recv_packet.debugString().c_str());
        #endif

        HEAD("Spoofing recv packet to destination...", RNS::LOG_TRACE);
        destination.receive(recv_packet);
        #endif

        HEAD("RNS Ready!", RNS::LOG_TRACE);
    }
    catch (const std::exception& e) {
        ERRORF("!!! Exception in reticulum_setup: %s", e.what());
    }*/
}

//-----------------------------------------------------------------------------

void Reticulum::stop() {
    INFO("Tearing down Reticulum...");

    try {
        RNS::Transport::persist_data();

        /*HEAD("Deregistering announce handler with Transport...", RNS::LOG_TRACE);
        RNS::Transport::deregister_announce_handler(announce_handler);*/

        HEAD("Deregistering Interface instances with Transport...", RNS::LOG_TRACE);
        if (udp_interface) {
            RNS::Transport::deregister_interface(udp_interface);
        }
        if (tcp_interface) {
            RNS::Transport::deregister_interface(tcp_interface);
        }
    }
    catch (const std::exception& e) {
        ERRORF("!!! Exception in Reticulum::stop: %s", e.what());
    }
}

//-----------------------------------------------------------------------------

void Reticulum::loop() {
    reticulum.loop();
}

//-----------------------------------------------------------------------------

std::string Reticulum::create_identity() {
    // Generates a fresh keypair. If you need a stable node identity across
    // restarts, persist the private key yourself (e.g. via microStore) and
    // restore it with Identity(false) + load_private_key(prv_bytes), the
    // same way the udp_announce example does.
    identity = RNS::Identity(); // <- use false arg if predefined identity
    return identity.hash().toHex();
}

//-----------------------------------------------------------------------------

std::string Reticulum::create_identity(const std::string& keyfile_path) {
    RNS::Bytes prv_bytes;
    std::ifstream in(keyfile_path, std::ios::binary);

    if (in.good()) {
        std::vector<uint8_t> raw((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        in.close();
        prv_bytes.assign(raw.data(), raw.size());
        identity = RNS::Identity(false);
        identity.load_private_key(prv_bytes);
        INFOF("Loaded existing Reticulum identity from %s", keyfile_path.c_str());
    }
    else {
        identity = RNS::Identity();
        prv_bytes = identity.get_private_key(); // confirm exact accessor name in Identity.h
        std::ofstream out(keyfile_path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(prv_bytes.data()), prv_bytes.size());
        out.close();
        INFOF("Generated new Reticulum identity, saved to %s", keyfile_path.c_str());
    }

    return identity.hash().toHex();
}

//-----------------------------------------------------------------------------

std::string Reticulum::create_destination(const char* app_name, const char* aspects) {
    if (!identity) {
        create_identity();
    }
    destination = RNS::Destination(
        identity,
        RNS::Type::Destination::IN,
        RNS::Type::Destination::SINGLE,
        app_name,
        aspects
    );
    destination.set_packet_callback(&Reticulum::on_packet_static);
    destination.set_proof_strategy(RNS::Type::Destination::PROVE_ALL);
    return destination.hash().toHex();
}

//-----------------------------------------------------------------------------

std::string Reticulum::announce(const std::string& app_data) {
    if (!destination) {
        throw std::runtime_error("Reticulum: no destination; call create_destination() first");
    }
    destination.announce(RNS::bytesFromString(app_data.c_str()));
    return destination.hash().toHex();
}

//-----------------------------------------------------------------------------

void Reticulum::send(const std::string& destination_hash_hex, const std::vector<uint8_t>& data) {
    if (!identity) {
        throw std::runtime_error("Reticulum: no identity; call create_identity() first");
    }

    RNS::Bytes dest_hash;
    dest_hash.assignHex(destination_hash_hex.c_str());

    RNS::Bytes payload(data.data(), data.size());

    // Building an OUT destination from a raw hash assumes we already have a
    // path to it (i.e. we've seen an announce from it, or path discovery has
    // resolved it via Transport). If sends are failing silently, check
    // Transport::has_path(dest_hash) / Transport::request_path(dest_hash)
    // before this call.
    RNS::Destination out_destination(
        identity,
        RNS::Type::Destination::OUT,
        RNS::Type::Destination::SINGLE,
        dest_hash
    );

    RNS::Packet packet(out_destination, payload);
    packet.pack();
    packet.send();
}

//-----------------------------------------------------------------------------

bool Reticulum::add_tcp_client(const std::string& host, uint16_t port) {
    try {
        auto* iface = new TCPInterface(host.c_str(), port);
        tcp_interface = iface;

        tcp_interface.mode(RNS::Type::Interface::MODE_GATEWAY);

        RNS::Transport::register_interface(tcp_interface);

        if (!tcp_interface.start()) {
            ERRORF("Failed to start TCPInterface to %s:%d", host.c_str(), port);
            return false;
        }

        INFOF("TCPInterface registered, connecting to %s:%d", host.c_str(), port);
        return true;
    }
    catch (const std::exception& e) {
        ERRORF("Exception in Reticulum::add_tcp_client: %s", e.what());
        return false;
    }
}

//-----------------------------------------------------------------------------

void Reticulum::set_packet_handler(std::function<void(const std::vector<uint8_t>&)> cb) {
    packet_callback = std::move(cb);
}

//-----------------------------------------------------------------------------

void Reticulum::on_packet_static(const RNS::Bytes& data, const RNS::Packet& packet) {
    if (instance_) {
        instance_->on_packet(data, packet);
    }
}

//-----------------------------------------------------------------------------

void Reticulum::on_packet(const RNS::Bytes& data, const RNS::Packet& /*packet*/) {
    if (packet_callback) {
        packet_callback(std::vector<uint8_t>(data.data(), data.data() + data.size()));
    }
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

}
