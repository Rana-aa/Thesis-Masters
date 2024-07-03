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

event zeek_init()
    {
    # Create the logging stream.
    Log::create_stream(PING::LOG, [$columns=PING::Info, $path="ping"]);
    }

event icmp_echo_request(c: connection, info: icmp_info, id: count, seq:count, payload:string)
{
  ping_src_set += {c$id$orig_h};
  if (c$id$orig_h in ping_src_set){
      ping_dst_set += {c$id$resp_h};
      
      if (|ping_dst_set| > 2) {
       Log::write(PING::LOG, [$sourceIP=c$id$orig_h]);
      }
   }

}