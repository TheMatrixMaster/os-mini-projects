#!/bin/bash

# Listens on localhost port 3001 (you can change this below)
# After connecting to this script, you can write shell commands to the socket
# The output of the shell command will be written back to the socket.

# Note, the process will only live for 10 seconds. You probably don't
# want to remove the timeout. The nc process will otherwise wait
# forever for a connection and cannot be interrupted with SIGINT.

FIFO_NAME=comp310-a2-${USER}

echo "Starting remote_shell for 10 seconds"
rm -f /tmp/${FIFO_NAME}; mkfifo /tmp/${FIFO_NAME}
cat /tmp/${FIFO_NAME} | /bin/sh -i 2>&1 | timeout 10 nc -l 0.0.0.0 3001 > /tmp/${FIFO_NAME}
echo "Done"

