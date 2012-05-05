import socket
import sys

HOST, PORT = "192.168.1.32", 5689
data = "message1\n"

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

try:
    sock.connect((HOST, PORT))
    sock.send(data)

    # Receive data from the server and shut down
    received = sock.recv(1024)
finally:
    sock.close()

print "Sent:     {0}".format(data)
print "Received: {0}".format(received)
