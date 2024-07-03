import os
import time
import socket
import subprocess

def check_flag_and_notify():
    shared_folder_path = '/data'  # Adjust the path according to your actual shared volume path
    flag_file_path = os.path.join(shared_folder_path, 'flag.txt')
    while True:
        if os.path.exists(flag_file_path):
            print("flag.txt found. Running speaker test and notifying PLC3.")
            notify_plc3()
            execute_speaker_test()
            os.remove(flag_file_path)  # Optionally remove the flag file after processing
        time.sleep(10)  # Check every 10 seconds

def execute_speaker_test():
    audio_file_path = '/src/key_sep_noiseless_Nonpadded.flac'
    volume = '0.72'
    subprocess.run(["play", "-v", volume, audio_file_path], check=True)

def notify_plc3():
    host = 'plc3'  # Assuming 'plc3' is the hostname or IP address
    port = 12345  # Port where PLC3 listens for commands, ensure it is open and listening
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.connect((host, port))
        s.sendall(b'update_status')  # Command that PLC3 will interpret to update the status

if __name__ == "__main__":
    check_flag_and_notify()
