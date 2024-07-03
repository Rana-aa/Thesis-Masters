#!/bin/sh

# Path to the ping.log file
PING_LOG="/pcap/ping.log"
MITM_LOG="/pcap/mitm.log"
DDOS_LOG="/pcap/ddos_detection.log"

# Path to the shared data directory where flag.txt will be created
SHARED_DATA="/data"

# Infinite loop to keep checking for ping.log
while true; do
    if [ -f "$PING_LOG" ] || [ -f "$MITM_LOG" ] || [ -f "$DDOS_LOG" ]; then
        # Create flag.txt in the shared data directory
        touch "${SHARED_DATA}/flag.txt"
        echo "Flag file created because ping.log exists."
    else
        echo "ping.log does not exist, no flag file created."
    fi
    # Wait for 120 seconds (2 minutes) before checking again
    sleep 120
done
