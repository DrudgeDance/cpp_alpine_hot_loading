FROM alpine:latest

# Set ARGs for user/group IDs (should match config.sh)
ARG USERNAME=developer
ARG USER_UID=1000
ARG USER_GID=$USER_UID

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
    linux-headers \
    # inotify-tools: Used to watch the scripts directory for changes and automatically set correct permissions
    # This ensures any new or modified shell scripts are automatically made executable for the developer
    inotify-tools

# Create and activate virtual environment for conan
RUN python3 -m venv /opt/venv && \
    chmod -R g+rwx /opt/venv

ENV PATH="/opt/venv/bin:$PATH"
ENV PYTHONPATH="/opt/venv/lib/python3.11/site-packages"
ENV VIRTUAL_ENV="/opt/venv"
# Disable the "(venv)" prefix in prompt
ENV VIRTUAL_ENV_DISABLE_PROMPT=1

# First layer: Install and upgrade pip
RUN /opt/venv/bin/pip3 install --no-cache-dir --upgrade pip

# Second layer: Install Conan (main tool)
RUN /opt/venv/bin/pip3 install --no-cache-dir conan==2.11.0

# Third layer: Install supporting packages
RUN /opt/venv/bin/pip3 install --no-cache-dir \
    distro==1.9.0 \
    urllib3==2.3.0 \
    patch-ng==1.18.1

# Create service group and user
RUN addgroup -g $SERVICE_GID $SERVICE_GROUP && \
    adduser -D -u $SERVICE_UID -G $SERVICE_GROUP $SERVICE_USER

# Create developer user with sudo privileges
RUN addgroup -g $USER_GID $USERNAME && \
    adduser -D -u $USER_UID -G $USERNAME $USERNAME && \
    echo "$USERNAME ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/$USERNAME && \
    chmod 0440 /etc/sudoers.d/$USERNAME

# Create workspace directory (permissions will be set by setup-permissions.sh)
RUN mkdir -p /workspaces

WORKDIR /workspaces/alpine_endpoint

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

USER $USERNAME 