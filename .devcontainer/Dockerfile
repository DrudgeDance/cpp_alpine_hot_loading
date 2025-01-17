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
    bash

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

# Switch to developer user
USER $USERNAME 