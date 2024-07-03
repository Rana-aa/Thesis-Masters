module PING;

export {
    # Append the value LOG to the Log::ID enumerable.
    redef enum Log::ID += { LOG };

    # Define a new type called Factor::Info.
    type Info: record {
        sourceIP:       addr &log;
        detection_time: time &log;
    };
}

# keep track of unique src addresses
global ping_src_set: set[addr] = set();
global ping_dst_set: set[addr] = set();

event zeek_init() {
    # Create the logging stream.
    Log::create_stream(PING::LOG, [$columns=PING::Info, $path="ping"]);
}

event icmp_echo_request(c: connection, info: icmp_info, id: count, seq: count, payload: string) {
    # Add the source host address to the ping_src_set
    add ping_src_set[c$id$orig_h];
    # Check if this source host has been seen before
    if (c$id$orig_h in ping_src_set) {
        # Add the destination host address to the ping_dst_set
        add ping_dst_set[c$id$resp_h];

        # If there are more than 3 distinct destinations that have been pinged by this source, log it
        if (|ping_dst_set| > 3) {
            local detection_time = network_time();
            print fmt("DETECTED PING SWEEP AT TIME %s", detection_time);
            Log::write(PING::LOG, [$sourceIP=c$id$orig_h, $detection_time=detection_time]);
        }
    }
}
