import logging

from ics_sim.Device import HIL
from Configs import TAG, PHYSICS, Connection

import numpy as np
from data import (
    # get_dataset,
    get_orig_dataset,
)

class FactorySimulation(HIL):
    def __init__(self):
        super().__init__('Factory', Connection.CONNECTION, 1000)
        self.init()
        #self.t_val = 0  # Initialize t_val as a class variable
        self.init()
        self.t_val = 0  # Initialize t_val as a class variable

        # Get the dataset once to determine its length
        _, test_data, _, _ = get_orig_dataset()
        test_arr = np.array(test_data.data)
        self.t = test_arr[:, 0]  # Load the t array
        self.x = test_arr[:, 1]  # Load the x array
        self.y = test_arr[:, 2]  # Load the y array
        self.data_length = len(self.t)  # Store the length of data arrays


    def _logic(self):
        elapsed_time = self._current_loop_time - self._last_loop_time
  
        current_index = self.t_val % self.data_length
        current_t = self.t[current_index]
        current_x = self.x[current_index]
        current_y = self.y[current_index]

        self.t_val += 1  # Increment t_val


        #current_t = self.t[self.t_val % self.t_length]  # Use modulo for cycling through t
        #self.t_val += 1  
             
        #self.t_val += 1
        #if self.t_val > 100:
        #    self.t_val = 0  # Reset t_val after reaching 10

        # update tank water level
        tank_water_amount = self._get(TAG.TAG_TANK_LEVEL_VALUE) * PHYSICS.TANK_LEVEL_CAPACITY
        if self._get(TAG.TAG_TANK_INPUT_VALVE_STATUS):
            tank_water_amount += PHYSICS.TANK_INPUT_FLOW_RATE * elapsed_time

        if self._get(TAG.TAG_TANK_OUTPUT_VALVE_STATUS):
            tank_water_amount -= PHYSICS.TANK_OUTPUT_FLOW_RATE * elapsed_time

        tank_water_level = tank_water_amount / PHYSICS.TANK_LEVEL_CAPACITY

        if tank_water_level > PHYSICS.TANK_MAX_LEVEL:
            tank_water_level = PHYSICS.TANK_MAX_LEVEL
            self.report('tank water overflowed', logging.WARNING)
        elif tank_water_level <= 0:
            tank_water_level = 0
            self.report('tank water is empty', logging.WARNING)

        # update tank water flow
        tank_water_flow = 0
        if self._get(TAG.TAG_TANK_OUTPUT_VALVE_STATUS) and tank_water_amount > 0:
            tank_water_flow = PHYSICS.TANK_OUTPUT_FLOW_RATE
            #u_value += 0.01

        # update bottle water
        '''
        if self._get(TAG.TAG_BOTTLE_DISTANCE_TO_FILLER_VALUE) > 1:
            bottle_water_amount = 0
            if self._get(TAG.TAG_TANK_OUTPUT_FLOW_VALUE):
                self.report('water is wasting', logging.WARNING)
        else:
            bottle_water_amount = self._get(TAG.TAG_BOTTLE_LEVEL_VALUE) * PHYSICS.BOTTLE_LEVEL_CAPACITY
            bottle_water_amount += self._get(TAG.TAG_TANK_OUTPUT_FLOW_VALUE) * elapsed_time

        bottle_water_level = bottle_water_amount / PHYSICS.BOTTLE_LEVEL_CAPACITY

        if bottle_water_level > PHYSICS.BOTTLE_MAX_LEVEL:
            bottle_water_level = PHYSICS.BOTTLE_MAX_LEVEL
            self.report('bottle water overflowed', logging.WARNING)

        # update bottle position
        bottle_distance_to_filler = self._get(TAG.TAG_BOTTLE_DISTANCE_TO_FILLER_VALUE)
        if self._get(TAG.TAG_CONVEYOR_BELT_ENGINE_STATUS):
            bottle_distance_to_filler -= elapsed_time * PHYSICS.CONVEYOR_BELT_SPEED
            bottle_distance_to_filler %= PHYSICS.BOTTLE_DISTANCE
        '''
        #v_value = 3
        #p_value = 6

        # update physical properties
        self._set(TAG.TAG_TANK_LEVEL_VALUE, tank_water_level)
        self._set(TAG.TAG_TANK_OUTPUT_FLOW_VALUE, tank_water_flow)
        #self._set(TAG.TAG_BOTTLE_LEVEL_VALUE, bottle_water_level)
        #self._set(TAG.TAG_BOTTLE_DISTANCE_TO_FILLER_VALUE, bottle_distance_to_filler)
        self._set(TAG.TAG_X_VALUE, current_x)
        self._set(TAG.TAG_Y_VALUE, current_y)
        self._set(TAG.TAG_T_VALUE, current_t)

    def init(self):
        initial_list = []
        for tag in TAG.TAG_LIST:
            initial_list.append((tag, TAG.TAG_LIST[tag]['default']))

        self._connector.initialize(initial_list)


    @staticmethod
    def recreate_connection():
        return True


if __name__ == '__main__':
    factory = FactorySimulation()
    factory.start()
