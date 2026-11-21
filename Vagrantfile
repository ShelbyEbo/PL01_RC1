# -*- mode: ruby -*-
# vi: set ft=ruby :
#
# RC-I 2026 — Projecto de Laboratório 1 (ddt — Base de Dados Distribuída)
#
# Infraestrutura:
#   boot   192.168.56.21  -> servidor de arranque UDP (boot_server, porta 58000)
#                            arranca automaticamente como serviço systemd
#   node1  192.168.56.22  -> nó ddt
#   node2  192.168.56.23  -> nó ddt
#   node3  192.168.56.24  -> nó ddt
#
# Todas as VMs têm compilador (gcc/make), depurador (gdb), valgrind e
# ferramentas de rede (nc, telnet, tcpdump). O código em /vagrant é copiado
# para /home/vagrant/ddt e compilado lá (nunca se reutilizam .o do host).
#
# Uso rápido:
#   vagrant up                         # cria e provisiona todas as VMs
#   vagrant ssh node1                  # entra num nó
#   cd ~/ddt && ./ddt -t 58001         # bootIP/bootport por omissão: 192.168.56.21:58000
#   vagrant provision --provision-with build   # recompila depois de alterar o código
#
# Variáveis de ambiente opcionais:
#   DDT_NODES=5 vagrant up             # número de VMs nó (por omissão 3, máx. 10)
#   DDT_BOX=ubuntu/jammy64 vagrant up  # outra box (por omissão bento/ubuntu-22.04, amd64 e arm64)

BOX        = ENV.fetch("DDT_BOX", "bento/ubuntu-22.04")
NUM_NODES  = [[ENV.fetch("DDT_NODES", "3").to_i, 1].max, 10].min
NET_PREFIX = "192.168.56"
BOOT_IP    = "#{NET_PREFIX}.21"
BOOT_PORT  = 58000

Vagrant.configure("2") do |config|
  config.vm.box = BOX
  config.vm.box_check_update = false

  # Pasta do projecto partilhada (só leitura do ponto de vista do fluxo de trabalho:
  # a compilação acontece numa cópia local dentro de cada VM).
  config.vm.synced_folder ".", "/vagrant"

  config.vm.provider "virtualbox" do |vb|
    vb.memory = 512
    vb.cpus   = 1
    vb.linked_clone = true
  end

  config.vm.provider "vmware_desktop" do |v|
    v.memory = 512
    v.cpus   = 1
  end

  config.vm.provider "parallels" do |prl|
    prl.memory = 512
    prl.cpus   = 1
  end

  # Pacotes comuns a todas as VMs (compilador, depurador, ferramentas de rede)
  config.vm.provision "packages", type: "shell",
    path: "scripts/provision.sh", args: ["packages"]

  # Copia /vagrant -> ~/ddt e compila ddt + boot_server (pode ser re-executado)
  config.vm.provision "build", type: "shell",
    path: "scripts/provision.sh", args: ["build"], privileged: false

  # ---------------------------------------------------------------- boot
  config.vm.define "boot", primary: true do |boot|
    boot.vm.hostname = "boot"
    boot.vm.network "private_network", ip: BOOT_IP

    boot.vm.provision "bootserver", type: "shell",
      path: "scripts/provision_boot.sh", args: [BOOT_PORT.to_s],
      run: "always"
  end

  # ---------------------------------------------------------------- nós
  (1..NUM_NODES).each do |i|
    config.vm.define "node#{i}" do |node|
      node.vm.hostname = "node#{i}"
      node.vm.network "private_network", ip: "#{NET_PREFIX}.#{21 + i}"
    end
  end
end
