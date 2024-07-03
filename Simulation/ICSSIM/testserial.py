import serial
import socket

# Read from serial port
def read_serial_port(port, baudrate, keyword):
    ser = serial.Serial(port, baudrate)
    while True:
        line = ser.readline().decode('utf-8').strip()
        print("Received:", line)
        if line == keyword:
            ser.close()
            return True

# Send command to PLC3 server
def send_command_to_server(command):
    host = 'localhost'  # Change to the actual host if not running locally
    port = 12345
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.connect((host, port))
        s.sendall(command.encode('utf-8'))
        print(f"Sent: {command}")

if __name__ == "__main__":
    # Configuration
    serial_port = 'COM6'
    baudrate = 500000
    keyword = 'done'

    # Main loop
    while True:
        if read_serial_port(serial_port, baudrate, keyword):
            print("FOUND")
            send_command_to_server('update_status_0')
            break
