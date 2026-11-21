#!/usr/bin/env bash
# Provisionamento comum a todas as VMs.
#   provision.sh packages  -> instala compilador, depurador e ferramentas de rede (root)
#   provision.sh build     -> copia /vagrant para ~/ddt e compila ddt e boot_server (vagrant)
set -euo pipefail

SRC_DIR=/vagrant
BUILD_DIR="${HOME}/ddt"

install_packages() {
    export DEBIAN_FRONTEND=noninteractive
    apt-get update -qq
    apt-get install -y -qq \
        build-essential gdb valgrind \
        netcat-openbsd telnet tcpdump iproute2 net-tools \
        rsync tmux
    # Hosts amigáveis para os testes
    grep -q "192.168.56.21 boot" /etc/hosts || cat >> /etc/hosts <<'EOF'
192.168.56.21 boot
192.168.56.22 node1
192.168.56.23 node2
192.168.56.24 node3
EOF
}

build() {
    mkdir -p "${BUILD_DIR}"
    # Copia só o que é preciso para compilar; nunca traz .o/binários do host
    rsync -a --delete \
        --include='Makefile' \
        --include='src/***' \
        --include='headers/***' \
        --exclude='*.o' \
        --exclude='*' \
        "${SRC_DIR}/" "${BUILD_DIR}/"
    cd "${BUILD_DIR}"
    make fclean >/dev/null
    make
    make bootserver
    echo "Compilado em ${BUILD_DIR}: $(ls ddt boot_server | tr '\n' ' ')"
}

case "${1:-}" in
    packages) install_packages ;;
    build)    build ;;
    *) echo "Uso: $0 {packages|build}" >&2; exit 1 ;;
esac
