import socket
import sys
import thread

def threadRecv(sock):
    while True:
        received = sock.recv(1024)
        if len(received) > 0:
            print "R: [%s]" % received

if __name__ == "__main__":
    HOST, PORT = "127.0.0.1", 5689
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
try:
    sock.connect((HOST, PORT))
    thread.start_new_thread(threadRecv, (sock,))
    while True:
        s = raw_input("# ")
        if sock.sendall(s + "\n") == None:
            print "S: [%s]" % s
finally:
    sock.close()
