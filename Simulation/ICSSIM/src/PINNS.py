# Import necessary components
from ics_sim.Device import PLC, SensorConnector, ActuatorConnector
from Configs import TAG, Connection, Controllers
import logging

class PLC4(PLC):
    # Define thresholds for sensors
    THRESHOLD_U = 10
    THRESHOLD_V = 15
    THRESHOLD_P = 20

    def __init__(self):
        # Initialize sensor connector
        sensor_connector = SensorConnector(Connection.CONNECTION)
        actuator_connector = ActuatorConnector(Connection.CONNECTION)

        # Initialize the PLC with sensor connectors and other necessary configurations
        super().__init__(3, sensor_connector, actuator_connector, TAG.TAG_LIST, Controllers.PLCs)

    def _logic(self):
        # Read sensor values
        x = self._get(TAG.TAG_X_VALUE)
        y = self._get(TAG.TAG_Y_VALUE)
        t = self._get(TAG.TAG_T_VALUE)

        # Check if any sensor value exceeds its threshold and report
    
    def predict(self,x1,y1,t1):
        if x1 == 1 and y1 == 2 and t1 == 3 :
            self._set(TAG.TAG_U_VALUE, 13)
            self._set(TAG.TAG_V_VALUE, 14)
            self._set(TAG.TAG_P_VALUE, 15)
        elif x1 == 1 and y1 == 2 and t1 == 7:
            self._set(TAG.TAG_U_VALUE, 26)
            self._set(TAG.TAG_V_VALUE, 27)
            self._set(TAG.TAG_P_VALUE, 28)


            
        

   

if __name__ == '__main__':
    plc4 = PLC4()
    plc4.set_record_variables(True)
    plc4.start()