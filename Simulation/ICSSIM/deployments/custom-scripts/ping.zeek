module PING;
export {
    # Append the value LOG to the Log::ID enumerable.
    redef enum Log::ID += { LOG };

    # Define a new type called Factor::Info.
    type Info: record {
        sourceIP:           addr &log;
        };

    }
# keep track of unique src addresses
global ping_src_set: set[addr] = set();
global ping_dst_set: set[addr] = set();
global local_subnets: set[subnet] = { 192.168.1.0/24, 192.168.0.0/24};

event zeek_init()
    {
    # Create the logging stream.
    Log::create_stream(PING::LOG, [$columns=PING::Info, $path="ping"]);
    }

event icmp_echo_request(c: connection, info: icmp_info, id: count, seq:count, payload:string)
{
  if (c$id$orig_h in local_subnets) {
        print fmt("something");
        # Log when a ping request is seen from 192.168.0.42
        Log::write(PING::LOG, [$sourceIP=c$id$orig_h]);

    }

}