module MITM;

export {
    # Append the value MITM_LOG to the Log::ID enumerable.
    redef enum Log::ID += { MITM_LOG };

    # Define a new type called Mitm::Info.
    type Info: record {
        address1: addr &log;
        detected_mac: string &log;
        original_mac: string &log;
        detection_time: time &log;
    };
}

# Define the global table to store addresses and MACs
global addr_to_mac: table[addr] of string = table();

event zeek_init() {
    # Create the logging stream for MitM detections.
    Log::create_stream(MITM::MITM_LOG, [$columns=MITM::Info, $path="mitm"]);

    # Initialize the addr_to_mac table with sample data
    addr_to_mac[192.168.0.21] = "02:42:c0:a8:00:15";
    addr_to_mac[192.168.0.11] = "02:42:c0:a8:00:0b";
    addr_to_mac[192.168.0.12] = "02:42:c0:a8:00:0c";
}

function detect_mitm(address1: addr, mac: string) {
    local old_mac = addr_to_mac[address1];
    if ( old_mac != "" && old_mac != mac ) {
        print fmt("DETECTED MITM OR REPLAY AT TIME %s", network_time());
        print fmt("Possible ARP Spoofing detected: %s is claimed by both %s and %s", address1, old_mac, mac);
        Log::write(MITM::MITM_LOG, [$address1=address1, $detected_mac=mac, $original_mac=old_mac, $detection_time=network_time()]);
    }
    addr_to_mac[address1] = mac;
}

# Detect ARP Spoofing
event arp_request(mac_src: string, mac_dst: string, SPA: addr, SHA: string, TPA: addr, THA: string) {
    detect_mitm(SPA, SHA);
}

event arp_reply(mac_src: string, mac_dst: string, SPA: addr, SHA: string, TPA: addr, THA: string) {
    detect_mitm(SPA, SHA);
}
