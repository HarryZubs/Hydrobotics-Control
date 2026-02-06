"""
Controller → 6x1 velocity vector → Ethernet (TCP server)

Packet format (per message):
    [uint64 length][6 x float32 values]

- Length is always 24 (6 * 4 bytes), but we keep the prefix
  so the protocol is easy to extend later.
"""

import socket
import struct
import sys
import time

import pygame


# --------------------- configuration ---------------------

HOST = "0.0.0.0"   # listen on all interfaces
PORT = 12345       # change if needed

UPDATE_RATE_HZ = 50          # how often to send commands
DEADZONE = 0.05              # ignore very small stick movements

# Mapping from joystick axes -> [X, Y, Z, RX, RY, RZ]
# Change these indices to match controller layout.
AXIS_MAP = {
    "X": 0,    # e.g. left stick horizontal
    "Y": 1,    # e.g. left stick vertical
    "Z": 4,    # e.g. triggers or right stick vertical
    "RX": 2,   # e.g. right stick horizontal
    "RY": 3,   # e.g. right stick vertical
    "RZ": 5,   # e.g. extra axis / spare
}

# Network packing: 6 float32 values
VECTOR_STRUCT = struct.Struct("!6f")   # network byte order, 6 floats
LENGTH_STRUCT = struct.Struct("!Q")    # uint64 length prefix

# ---------------------------------------------------------


def init_joystick() -> pygame.joystick.Joystick:
    pygame.init()
    pygame.joystick.init()

    if pygame.joystick.get_count() < 1:
        print("No joystick/gamepad detected. Please plug one in.", file=sys.stderr)
        sys.exit(1)

    js = pygame.joystick.Joystick(0)
    js.init()

    print(f"Using joystick: {js.get_name()}")
    print(f"  Axes:    {js.get_numaxes()}")
    print(f"  Buttons: {js.get_numbuttons()}")
    print(f"  Hats:    {js.get_numhats()}")

    return js


def apply_deadzone(value: float, dz: float) -> float:
    """Zero small values to remove noise."""
    if abs(value) < dz:
        return 0.0
    return value


def read_velocity_vector(js: pygame.joystick.Joystick) -> list[float]:
    """
    Read current joystick state and map to [X, Y, Z, RX, RY, RZ] in [-1, 1].

    Can add scaling here later if real units are wanted (m/s, rad/s, etc.).
    """
    pygame.event.pump()   # process internal events so axes update

    # Fetch axes with fallback if a particular axis index doesn't exist
    def get_axis_safe(index: int) -> float:
        if 0 <= index < js.get_numaxes():
            return js.get_axis(index)
        return 0.0

    X  = apply_deadzone(get_axis_safe(AXIS_MAP["X"]),  DEADZONE)
    Y  = apply_deadzone(get_axis_safe(AXIS_MAP["Y"]),  DEADZONE)
    Z  = apply_deadzone(get_axis_safe(AXIS_MAP["Z"]),  DEADZONE)
    RX = apply_deadzone(get_axis_safe(AXIS_MAP["RX"]), DEADZONE)
    RY = apply_deadzone(get_axis_safe(AXIS_MAP["RY"]), DEADZONE)
    RZ = apply_deadzone(get_axis_safe(AXIS_MAP["RZ"]), DEADZONE)

    return [X, Y, Z, RX, RY, RZ]


def start_server(host: str, port: int) -> socket.socket:
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    # Allow quick restart after closing.
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind((host, port))
    srv.listen(1)
    print(f"Controller server listening on {host}:{port}")
    return srv


def send_vector(conn: socket.socket, vector: list[float]) -> None:
    """
    Serialize the 6-element vector and send it with a length prefix.
    """
    payload = VECTOR_STRUCT.pack(*vector)
    header = LENGTH_STRUCT.pack(len(payload))
    conn.sendall(header + payload)


# --------------------------- main ---------------------------

def main() -> None:
    joystick = init_joystick()
    server_socket = start_server(HOST, PORT)

    print("Waiting for client to connect...")
    conn, addr = server_socket.accept()
    print(f"Client connected from {addr}")

    clock = pygame.time.Clock()

    try:
        while True:
            vector = read_velocity_vector(joystick)

            # Optional: print occasionally for debugging
            # print("Vector:", ["{:+.2f}".format(v) for v in vector])

            try:
                send_vector(conn, vector)
            except (BrokenPipeError, ConnectionResetError):
                print("Client disconnected.")
                break

            # Limit update rate
            clock.tick(UPDATE_RATE_HZ)

    except KeyboardInterrupt:
        print("Shutting down (Ctrl+C).")

    finally:
        conn.close()
        server_socket.close()
        pygame.quit()


if __name__ == "__main__":
    main()
