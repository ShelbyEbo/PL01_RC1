# PL01_RC1 — Distributed Database (DDT)

## Descrição do Projecto

Este projecto consiste no desenvolvimento de uma aplicação de base de dados distribuída, organizada através de uma rede de nós interligados em anel (*Ring Network*).

A aplicação permite que os nós entrem e saiam da rede, comuniquem entre si e realizem operações de pesquisa distribuída.

O projecto está implementado em C e utiliza comunicação através dos protocolos UDP e TCP.

## Estrutura do Projecto

```text
PL01_RC1/
├── Makefile
├── Vagrantfile              # infraestrutura (1 servidor de arranque + 3 nós)
├── TESTES.md                # plano de testes detalhado
├── scripts/
│   ├── provision.sh         # instala pacotes e compila dentro das VMs
│   └── provision_boot.sh    # instala o boot_server como serviço systemd
├── headers/
│   └── header.h
└── src/
    ├── main.c
    ├── process.c
    ├── bootstrap.c
    ├── join.c
    ├── leave.c
    ├── search.c
    ├── server.c
    └── boot_server.c        # servidor de arranque UDP (executável separado)
```

## Descrição dos Ficheiros

### `main.c` — Ponto de entrada da aplicação

Contém a função `main()`, responsável pela inicialização da aplicação.

Cria e inicializa a estrutura que representa um nó da rede. Em seguida, aguarda a introdução de comandos pelo utilizador e encaminha-os para a função `process_command()`.

### `process.c` — Processamento de comandos

Responsável por interpretar os comandos introduzidos pelo utilizador.

Identifica o comando solicitado e encaminha a execução para a função correspondente, como a entrada na rede (`join`), a saída da rede (`leave`) ou a pesquisa (`search`).

### `bootstrap.c` — Comunicação com o Bootstrap Server

Responsável pela comunicação UDP com o Bootstrap Server.

Permite ao nó obter informações sobre a rede e registar ou remover a sua participação, de acordo com o protocolo definido no projecto.

### `join.c` — Entrada de um nó na rede

Implementa a lógica necessária para um nó entrar na rede distribuída.

É responsável por coordenar o processo de entrada e estabelecer as relações necessárias com os outros nós do anel.

### `leave.c` — Saída de um nó da rede

Implementa o processo de saída de um nó da rede.

Coordena a actualização das ligações entre os nós vizinhos, de forma a preservar a estrutura do anel.

### `search.c` — Pesquisa distribuída

Implementa a lógica de pesquisa na rede distribuída.

Permite encaminhar pedidos de pesquisa entre os nós, de acordo com o protocolo definido no projecto.

### `server.c` — Servidor de comunicação TCP

Implementa o servidor TCP de cada nó.

É responsável por receber conexões de outros nós, processar mensagens recebidas e enviar respostas de acordo com o protocolo de comunicação.

### `boot_server.c` — Servidor de arranque (UDP)

Executável separado (`boot_server`). Guarda, para cada anel, o identificador e a localização do seu nó de arranque, e responde às mensagens `BQRY`, `REG` e `UNR`.

## Outros Ficheiros

### `header.h` — Definições e declarações partilhadas

Contém as definições utilizadas pelos diferentes módulos do projecto, incluindo estruturas de dados, constantes, bibliotecas e protótipos de funções.

### `Makefile` — Compilação do projecto

Automatiza o processo de compilação dos ficheiros-fonte e a geração do executável `ddt`.

Disponibiliza os seguintes comandos:

| Comando | Descrição |
|---|---|
| `make` | Compila o `ddt` |
| `make bootserver` | Compila o servidor de arranque `boot_server` |
| `make clean` | Remove os ficheiros objecto |
| `make fclean` | Remove os ficheiros objecto e os executáveis |
| `make re` | Limpa e recompila o projecto |

## Pôr o projecto a funcionar

### Requisitos (no host)

- [Vagrant](https://developer.hashicorp.com/vagrant/install)
- Um *provider*: VirtualBox 7.1+ (Intel ou Apple Silicon), VMware Desktop ou Parallels (este último precisa de `vagrant plugin install vagrant-parallels`)

### Infraestrutura

| VM      | IP              | Papel                                          |
|---------|-----------------|------------------------------------------------|
| `boot`  | 192.168.56.21   | `boot_server` na porta 58000/udp (arranca sozinho) |
| `node1` | 192.168.56.22   | nó `ddt`                                       |
| `node2` | 192.168.56.23   | nó `ddt`                                       |
| `node3` | 192.168.56.24   | nó `ddt`                                       |

O IP e a porta do servidor de arranque são os valores por omissão do `ddt`, por isso nos nós não é preciso passar `-i` nem `-p`.

### 1. Criar as VMs

Na raiz do repositório:

```bash
vagrant up          # cria as VMs, instala gcc/gdb/valgrind/nc/tcpdump, compila e arranca o boot_server
vagrant status      # todas devem estar "running"
```

Opções:

```bash
DDT_NODES=5 vagrant up             # mais VMs nó (por omissão 3, máximo 10)
DDT_BOX=ubuntu/jammy64 vagrant up  # outra box (por omissão bento/ubuntu-22.04)
```

### 2. Correr os nós

Abrir um terminal para cada nó:

```bash
vagrant ssh node1
cd ~/ddt
./ddt -t 58001
```

```bash
vagrant ssh node2
cd ~/ddt
./ddt -t 58002
```

Sintaxe completa: `./ddt [-t ringport] [-i bootIP] [-p bootport]`.

Logs do servidor de arranque, num terminal à parte:

```bash
vagrant ssh boot -c 'journalctl -u boot_server -f'
```

### 3. Comandos do `ddt`

| Comando | Descrição |
|---------|-----------|
| `join x i` | Entra no anel `x` com o identificador `i` (0..63), usando o servidor de arranque |
| `join x i succ succ.IP succ.TCP` | Entra no anel `x` ligando-se directamente ao sucessor indicado (sem pesquisa) |
| `show` | Mostra o anel, o próprio id, o predecessor e o sucessor |
| `search k` | Mostra o nó responsável pelo identificador `k` |
| `leave` | Sai do anel |
| `exit` | Sai do anel (se estiver num) e fecha a aplicação |

### 4. Depois de alterar o código

O código é copiado de `/vagrant` para `~/ddt` dentro de cada VM e compilado lá. Depois de uma alteração:

```bash
vagrant provision --provision-with build   # recompila em todas as VMs
vagrant provision boot                     # recompila e reinicia o boot_server
```

### 5. Desligar / apagar

```bash
vagrant halt         # desliga as VMs
vagrant destroy -f   # apaga as VMs
```

## Como testar

### Teste rápido (anel de 3 nós)

1. Em **node1**: `./ddt -t 58001` e depois `join 1 10`
2. Em **node2**: `./ddt -t 58002` e depois `join 1 30`
3. Em **node3**: `./ddt -t 58003` e depois `join 1 50`
4. `show` nos três. O resultado esperado é o anel 10 → 30 → 50 → 10:
   - node1: pred 50, succ 30
   - node2: pred 10, succ 50
   - node3: pred 30, succ 10
5. Em **node1**:
   - `search 40` → `(50, 192.168.56.24, 58003)`
   - `search 20` → `(30, 192.168.56.23, 58002)`
   - `search 5` → `(10, 192.168.56.22, 58001)`
6. `leave` em node1, que é o nó de arranque. Verificar que o node2 passou a ser o nó de arranque:

   ```bash
   echo "BQRY 1" | nc -u -w1 192.168.56.21 58000    # esperado: BRSP 1 30 192.168.56.23 58002
   ```

7. `exit` em todos. No fim, `BQRY 1` deve responder `EMPTY`.

### Ferramentas úteis

```bash
# Falar directamente com o servidor de arranque (UDP)
echo "BQRY 1" | nc -u -w1 192.168.56.21 58000

# Fingir ser um nó que quer aderir (TCP); a resposta esperada é SUCC ...
telnet 192.168.56.22 58001
ID 40

# Ver as mensagens do protocolo a passar
sudo tcpdump -i any -A -n udp port 58000                    # na VM boot
sudo tcpdump -i any -A -n 'tcp portrange 58001-58010'       # num nó

# Memória
valgrind --leak-check=full --track-fds=yes ./ddt -t 58001
```

Para ter mais nós sem criar VMs extra, pode-se correr vários `ddt` na mesma VM com portas diferentes (`-t 58011`, `-t 58012`, …).

### Plano de testes completo

O ficheiro [`TESTES.md`](TESTES.md) tem todos os testes, cada um com o comando e o resultado esperado:

- compilação
- servidor de arranque
- invocação do `ddt`
- interface de utilizador
- `join` (directo e com pesquisa)
- `search`
- `leave`
- robustez
- valgrind/gdb
- checklist final antes da entrega

## Compilação sem Vagrant (local)

Para compilar e testar directamente na máquina, executar na raiz do repositório:

```bash
make
make bootserver
```

Após a compilação, serão gerados os executáveis `ddt` e `boot_server`.

Para testar tudo na mesma máquina via loopback:

```bash
./boot_server -p 58000 &
./ddt -t 58001 -i 127.0.0.1 -p 58000
```
