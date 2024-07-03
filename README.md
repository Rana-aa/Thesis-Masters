# Thesis-Masters
![sim_net](https://github.com/Rana-aa/Thesis-Masters/assets/174622322/f947db12-4780-4a96-a1c4-1a690acd5de1)
This repository contains the necessary code and resources to run simulations using Grid Frequency Modulation (GFM), Physics-Informed Neural Networks (PINNs), and other related modules. The structure of the repository is organized into different folders for clarity and ease of use.

## Repository Structure

- **/GFM**: Contains the Grid Frequency Modulation (GFM) related code.
- **/PINNS**: Contains the Physics-Informed Neural Networks (PINNs) related code.
- **/ICSSIM**: Contains the main simulation code and deployment scripts.

## Getting Started

To set up and run the simulation, follow the instructions below:

### Prerequisites

1. Ensure you have all the necessary software and dependencies installed on your system like the pulseaudio server.
2. Connect the microcontroller to your laptop to generate the audio signal required for the simulation.

### Running the Simulation

1. Navigate to the `ICSSIM/deployments` directory:
    ```bash
    cd ICSSIM/deployments
    ```
    
2. Run the pulseaudio server in a seperate powershell terminal
   ```bash
   .\pulseaudio-1.1\bin\pulseaudio.exe --use-pid-file=false
   ```
3. Run the initialization script:
    ```bash
    ./init.sh
    ```




