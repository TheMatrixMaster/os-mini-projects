#!/bin/bash
# Simple tcp server using netcat
#  - depending on the netcat version either use nc -l 5555 or nc -l -p 5555
#  - verify with `telnet locahhost 5555`
#  - quit the telnet with `ctrl-]` and then type quit
#  - the while loop is there so reopen the port after a client has disconnected
#  - supports only one client at a time
PORT=5555;
while :; do nc -l -p $PORT | tee  output.log; sleep 1; done
