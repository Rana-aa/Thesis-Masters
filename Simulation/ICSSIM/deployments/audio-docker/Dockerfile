FROM ubuntu:20.04

RUN mkdir src

COPY ./src/ ./src/

RUN apt update && apt install -y bash pulseaudio alsa-utils sox libsox-fmt-all python3

ENV PULSE_SERVER=host.docker.internal

WORKDIR /src

# Copy the server script
#COPY server.py /app/server.py

# Set the working directory
#WORKDIR /app

# Command to run the script
#CMD ["python3", "server.py"]