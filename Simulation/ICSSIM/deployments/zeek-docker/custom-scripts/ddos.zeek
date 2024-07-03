module DDoS_Detection;

export {
    # Append the value DDoS_LOG to the Log::ID enumerable.
    redef enum Log::ID += { DDoS_LOG };

    # Define a new type called DDoS_Detection::Info for logging purposes.
    type Info: record {
        src_ip: addr &log;
        dst_ip: addr &log;
        packet_count: count &log;
        detection_time: time &log;
    };
}

global target_traffic: table[addr] of count = table();

# Initialize log streams and global variables
event zeek_init() {
    # Create a logging stream for DDoS detection
    Log::create_stream(DDoS_Detection::DDoS_LOG, [$columns=DDoS_Detection::Info, $path="ddos_detection"]);

    # Initialize 
    target_traffic[192.168.0.21] = 0;
    target_traffic[192.168.0.22] = 0;
    target_traffic[192.168.0.11] = 0;
    target_traffic[192.168.0.12] = 0;
    #target_traffic[192.168.0.42] = 0;
}

# Event that gets triggered for each new connection
event new_connection(c: connection) {
    # Update traffic count for the destination IP
    ++target_traffic[c$id$resp_h];

    # Check if the traffic exceeds the defined threshold
    if (target_traffic[c$id$resp_h] > 500) {
        print fmt("DETECTED DDOS AT TIME %s", network_time());
        Log::write(DDoS_Detection::DDoS_LOG, [$src_ip=c$id$orig_h, $dst_ip=c$id$resp_h, $packet_count=target_traffic[c$id$resp_h], $detection_time=network_time()]);
        print fmt("Possible DDoS attack detected: %s is receiving high traffic from %s", c$id$resp_h, c$id$orig_h);
    }
}

# Event triggered when a connection state changes
event connection_state_remove(c: connection) {
    # Decrease the traffic count for the destination IP
    if (c$id$resp_h in target_traffic) {
        --target_traffic[c$id$resp_h];
    }
}


