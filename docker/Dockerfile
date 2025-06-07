# Use the espressif/idf:release-v5.5 as the base image
FROM espressif/idf:release-v5.5

# Install Node.js 20.x and Python dependencies
RUN apt-get update && \
    apt-get install -y curl python3-pip git vim && \
    curl -fsSL https://deb.nodesource.com/setup_20.x | bash - && \
    apt-get install -y nodejs && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

# Install bitaxetool
RUN pip3 install bitaxetool --break-system-packages

# Clone the esp-idf-nvs-partition-gen repository
RUN git clone https://github.com/espressif/esp-idf-nvs-partition-gen.git /tmp/esp-idf-nvs-partition-gen

# Install nvs_partition_gen.py globally
RUN cp /tmp/esp-idf-nvs-partition-gen/esp_idf_nvs_partition_gen/nvs_partition_gen.py /usr/local/bin/ && \
    chmod +x /usr/local/bin/nvs_partition_gen.py

# Clean up temporary files
RUN rm -rf /tmp/esp-idf-nvs-partition-gen

# Set working directory
WORKDIR /home/ubuntu/project

# Switch to the unprivileged user
USER ubuntu

