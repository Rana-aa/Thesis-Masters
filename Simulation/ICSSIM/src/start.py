import random

from pyModbusTCP.server import ModbusServer

from Configs import TAG
from HMI1 import HMI1
from FactorySimulation import FactorySimulation
from PLC1 import PLC1
#from PLC2 import PLC2
from PLC3 import PLC3

import memcache
from Configs import Connection, TAG

from ics_sim.protocol import ProtocolFactory
from ics_sim.connectors import FileConnector, ConnectorFactory

factory = FactorySimulation()
factory.start()


plc1 = PLC1()
plc1.set_record_variables(True)
plc1.start()


#plc2 = PLC2()
#plc2.set_record_variables(True)
#plc2.start()

plc3 = PLC3()
plc3.set_record_variables(True)
plc3.start()

"""

connector = ConnectorFactory.build(Connection.File_CONNECTION)
"""






