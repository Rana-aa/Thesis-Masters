import socket
import subprocess

def execute_speaker_test():
     # Path to your audio file
    audio_file_path = '/src/sample.mp3'
    # Command to play the audio file using SoX
    subprocess.run(["play", audio_file_path], check=True)
    # Command to execute speaker-test
    #subprocess.run(["speaker-test", "-t", "wav", "-c", "2"], check=True)

def server():
    host = '0.0.0.0'  # Listen on all network interfaces
    port = 12345      # Port to listen on (non-privileged ports are > 1023)

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind((host, port))
        s.listen()
        print(f"Listening on {host}:{port}")

        while True:
            conn, addr = s.accept()
            with conn:
                print(f"Connected by {addr}")
                while True:
                    data = conn.recv(1024)
                    if not data:
                        break
                    command = data.decode('utf-8')
                    print(f"Received command: {command}")
                    if command == 'run speaker-test':
                        execute_speaker_test()

if __name__ == "__main__":
    server()
