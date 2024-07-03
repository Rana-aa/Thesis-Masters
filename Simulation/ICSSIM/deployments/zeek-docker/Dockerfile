# Start from the official Zeek image
FROM blacktop/zeek

RUN mkdir /usr/local/zeek/share/zeek/policy/custom-scripts
 
# Copy your custom Zeek script into the image
COPY custom-scripts /usr/local/zeek/share/zeek/policy/custom-scripts

COPY check-and-flag.sh /usr/local/bin/check-and-flag.sh
RUN chmod +x /usr/local/bin/check-and-flag.sh

# Make sure Zeek loads your custom script
RUN echo -e "\n@load custom-scripts" >> /usr/local/zeek/share/zeek/site/local.zeek
RUN echo -e "\n@load policy/protocols/conn/mac-logging" >> /usr/local/zeek/share/zeek/site/local.zeek
# Clear the ENTRYPOINT
ENTRYPOINT []
CMD ["zeek"]