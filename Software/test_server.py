import socket
import struct
import time

HOST = '0.0.0.0'  # listen on all interfaces (or you can specify your laptop IP)
PORT = 5005

def make_vector():
    # Example vector; you can generate this or get it from somewhere else
    X, Y, Z = 1.0, 2.0, 3.0
    RX, RY, RZ = 0.1, 0.2, 0.3
    return [X, Y, Z, RX, RY, RZ]

def pack_vector(vec):
    # pack as 6 doubles (float64). Change format if you want another precision.
    return struct.pack('!6d', *vec)  # network byte order (big-endian)

def run_server():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind((HOST, PORT))
        s.listen(1)
        print(f"Server listening on {HOST}:{PORT}")
        conn, addr = s.accept()
        with conn:
            print("Connected by", addr)
            try:
                while True:
                    vec = make_vector()
                    data = pack_vector(vec)
                    conn.sendall(data)
                    print("Sent vector:", vec)
                    time.sleep(1.0)  # send every second
            except (BrokenPipeError, ConnectionResetError):
                print("Client disconnected")

if __name__ == '__main__':
    run_server()
