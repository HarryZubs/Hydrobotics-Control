import socket

ESP32_IP = "192.168.4.1"   # Default AP IP for softAP mode
PORT = 1234

print("Connecting to ESP32...")

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect((ESP32_IP, PORT))

print("Connected! Waiting for data...\n")

while True:
    data = s.recv(1024)

    if not data:
        continue  # connection alive but nothing received

    # Decode and remove whitespace
    line = data.decode().strip()

    print("Received:", line)