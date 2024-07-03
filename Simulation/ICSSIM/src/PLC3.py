import logging
from pathlib import Path
import random
import numpy as np
import torch
import socket
import threading
from ics_sim.Device import PLC, SensorConnector, ActuatorConnector
from Configs import TAG, Connection, Controllers
from model import Pinn
from data import get_orig_dataset
from trainer import Trainer

class PLC3(PLC):
    THRESHOLD_U = 10
    THRESHOLD_V = 15
    THRESHOLD_P = 20

    def __init__(self):
        sensor_connector = SensorConnector(Connection.CONNECTION)
        actuator_connector = ActuatorConnector(Connection.CONNECTION)
        super().__init__(3, sensor_connector, actuator_connector, TAG.TAG_LIST, Controllers.PLCs)

        # Load data 
        _, test_data, min_x, max_x = get_orig_dataset()
        self.test_arr = np.array(test_data.data)
        self.u_pred = np.loadtxt('u_pred.txt')
        self.v_pred = np.loadtxt('v_pred.txt')
        self.p_pred = np.loadtxt('p_pred.txt')
        self.t_length = len(self.p_pred)  # Assuming all arrays are of same length
    

    def handle_client(self, conn, addr):
        print(f"Connected by {addr}")
        with conn:
            while True:
                data = conn.recv(1024)
                if not data:
                    break
                command = data.decode('utf-8')
                if command == 'update_status':
                    print("Updating pinns_status to 1")
                    self._set(TAG.TAG_PINNS_STATUS, 1)
                elif command == 'update_status_0':
                    print("Updating pinns_status to 0")
                    self._set(TAG.TAG_PINNS_STATUS, 0)
        print(f"Connection closed by {addr}")

    def start_server(self):
        host = ''  # Empty string means listen on all network interfaces
        port = 12345  # Same port as specified in the notify function
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.bind((host, port))
            s.listen()
            print(f"PLC3 listening on {port}")
            while True:
                conn, addr = s.accept()
                client_thread = threading.Thread(target=self.handle_client, args=(conn, addr))
                client_thread.start()
 

    def _logic(self):
        t = int(self._get(TAG.TAG_T_VALUE)) % self.t_length  # Convert to int and ensure it wraps around

        # Use t as index to fetch predictions
        u_val = float(self.u_pred[t].item()) if isinstance(self.u_pred[t], np.generic) else float(self.u_pred[t])
        v_val = float(self.v_pred[t].item()) if isinstance(self.v_pred[t], np.generic) else float(self.v_pred[t])
        p_val = float(self.p_pred[t].item()) if isinstance(self.p_pred[t], np.generic) else float(self.p_pred[t])

        u_val2 = float(self.test_arr[:, 4][t])
        v_val2 = float(self.test_arr[:, 5][t])
        p_val2 = float(self.test_arr[:, 3][t])

        # Set TAG values based on the current predictions
        pinn_state = self._get(TAG.TAG_PINNS_STATUS)
        if pinn_state:
            self._set(TAG.TAG_U_VALUE, u_val)
            self._set(TAG.TAG_V_VALUE, v_val)
            self._set(TAG.TAG_P_VALUE, p_val)
        else:
            self._set(TAG.TAG_U_VALUE, u_val2)
            self._set(TAG.TAG_V_VALUE, v_val2)
            self._set(TAG.TAG_P_VALUE, p_val2)
            #self.notify_other_container()



if __name__ == '__main__':
    plc3 = PLC3()
    plc3.set_record_variables(True)
    #plc3.start()
    # Create a thread for the PLC logic
    plc_logic_thread = threading.Thread(target=plc3.start)
    plc_logic_thread.start()

    # Create and start the server thread
    server_thread = threading.Thread(target=plc3.start_server)
    server_thread.start()
