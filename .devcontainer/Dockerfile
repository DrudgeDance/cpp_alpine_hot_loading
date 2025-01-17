FROM alpine:latest

# Set ARGs
ARG USERNAME=developer
ARG USER_UID=1000
ARG USER_GID=$USER_UID
ARG WORKSPACE_NAME=cppdev
ARG SERVICE_USER=appuser
ARG SERVICE_GROUP=appgroup
ARG SERVICE_UID=2000
ARG SERVICE_GID=2000

# Install necessary packages
RUN apk update && apk add --no-cache \
    build-base \
    cmake \
    git \
    sudo \
    bash \
    python3 \
    python3-dev \
    py3-pip \
    musl-dev \
    boost-dev \
    boost-static \
    linux-headers

# Create and activate virtual environment for conan
RUN python3 -m venv /opt/venv
ENV PATH="/opt/venv/bin:$PATH"

# Install conan in virtual environment
RUN pip3 install --no-cache-dir conan

# Create service group and user
RUN addgroup -g $SERVICE_GID $SERVICE_GROUP && \
    adduser -D -u $SERVICE_UID -G $SERVICE_GROUP $SERVICE_USER

# Create developer user with sudo privileges
RUN addgroup -g $USER_GID $USERNAME && \
    adduser -D -u $USER_UID -G $USERNAME $USERNAME && \
    echo "$USERNAME ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/$USERNAME && \
    chmod 0440 /etc/sudoers.d/$USERNAME

# Create workspace directory and set permissions
RUN mkdir -p /workspace && \
    chown $USERNAME:$USERNAME /workspace

WORKDIR /workspace

# Set up conan for the developer user
USER $USERNAME

# Create and configure conan profile
RUN conan profile detect --force && \
    echo "[settings]" > ~/.conan2/profiles/default && \
    echo "arch=armv8" >> ~/.conan2/profiles/default && \
    echo "build_type=Release" >> ~/.conan2/profiles/default && \
    echo "compiler=gcc" >> ~/.conan2/profiles/default && \
    echo "compiler.cppstd=17" >> ~/.conan2/profiles/default && \
    echo "compiler.libcxx=libstdc++11" >> ~/.conan2/profiles/default && \
    echo "compiler.version=13" >> ~/.conan2/profiles/default && \
    echo "os=Linux" >> ~/.conan2/profiles/default && \
    echo "[options]" >> ~/.conan2/profiles/default && \
    echo "*:shared=False" >> ~/.conan2/profiles/default

# Copy and setup auto-chmod script
COPY --chown=$USERNAME:$USERNAME scripts/auto-chmod.sh /home/$USERNAME/.local/scripts/
RUN echo 'source ~/.local/scripts/auto-chmod.sh' >> /home/$USERNAME/.bashrc

USER $USERNAME 