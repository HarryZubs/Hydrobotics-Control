import socket
import struct
import serial  # pyserial
import time

# Configuration
LAPTOP_IP = '192.168.1.100'  # change to your laptop's IP on the same network
LAPTOP_PORT = 5005

# Serial settings
SERIAL_DEVICE = '/dev/ttyTHS0'  # or whichever UART you’re using
BAUD_RATE = 115200  # you can choose appropriate baud

def run_client():
    # Open serial port
    ser = serial.Serial(SERIAL_DEVICE, BAUD_RATE, timeout=1)
    print(f"Opened serial {SERIAL_DEVICE} at {BAUD_RATE} baud")

    # Connect to server
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        print(f"Connecting to {LAPTOP_IP}:{LAPTOP_PORT} …")
        s.connect((LAPTOP_IP, LAPTOP_PORT))
        print("Connected to server")

        # The size of the packed vector
        vec_size = struct.calcsize('!6d')
        print("Expecting", vec_size, "bytes per vector")

        try:
            while True:
                # Receive exactly vec_size bytes
                buf = b''
                while len(buf) < vec_size:
                    chunk = s.recv(vec_size - len(buf))
                    if not chunk:
                        raise ConnectionError("Server closed connection")
                    buf += chunk

                vec = struct.unpack('!6d', buf)
                print("Received vector:", vec)

                # Convert vector to some serial format, e.g., as comma-separated string
                msg = ','.join(f"{v:.6f}" for v in vec) + '\n'
                ser.write(msg.encode('utf-8'))
                print("Sent over serial:", msg.strip())

                # Optional: read response from microcontroller
                time.sleep(0.01)  # wait a bit
                if ser.in_waiting:
                    resp = ser.readline().decode('utf-8', errors='ignore').strip()
                    print("Response from MCU:", resp)

        except (ConnectionError, KeyboardInterrupt) as e:
            print("Connection ended:", e)
        finally:
            ser.close()
            print("Serial closed")

if __name__ == '__main__':
    run_client()
