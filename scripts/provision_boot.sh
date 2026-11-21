#!/usr/bin/env bash
# Instala e (re)inicia o servidor de arranque UDP como serviço systemd na VM "boot".
#   provision_boot.sh [porta]   (por omissão 58000)
set -euo pipefail

PORT="${1:-58000}"
BIN=/home/vagrant/ddt/boot_server

if [ ! -x "${BIN}" ]; then
    echo "Erro: ${BIN} não existe. Corre 'vagrant provision boot --provision-with build'." >&2
    exit 1
fi

install -m 0755 "${BIN}" /usr/local/bin/boot_server

cat > /etc/systemd/system/boot_server.service <<EOF
[Unit]
Description=RC-I ddt - Servidor de arranque UDP
After=network-online.target
Wants=network-online.target

[Service]
# stdbuf -oL: stdout linha-a-linha para os logs aparecerem logo no journal
ExecStart=/usr/bin/stdbuf -oL /usr/local/bin/boot_server -p ${PORT}
Restart=on-failure
RestartSec=1
User=vagrant
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
EOF

systemctl daemon-reload
systemctl enable boot_server.service >/dev/null
systemctl restart boot_server.service
sleep 1
systemctl --no-pager --lines=0 status boot_server.service || true
echo "Servidor de arranque activo em 192.168.56.21:${PORT}/udp"
echo "Logs: vagrant ssh boot -c 'journalctl -u boot_server -f'"
