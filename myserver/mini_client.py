import socket
import sys
import thread

def threadRecv(sock):
    while True:
        received = sock.recv(1024)
        if len(received) > 0:
            print "\x1b[32;01mR: [%s]\x1b[31;01m" % received[:-1]

if __name__ == "__main__":
    sys.stdout.write("\x1b[31;01m")
    HOST, PORT = "127.0.0.1", 5689
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
try:
    sock.connect((HOST, PORT))
    thread.start_new_thread(threadRecv, (sock,))
    while True:
        s = raw_input()
        if sock.sendall(s + "\n") == None:
            print "\x1b[32;01mS: [%s]\x1b[31;01m" % s
finally:
    sock.close()
