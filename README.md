# PL01_RC1 — Distributed Database (DDT)

## Descrição do Projecto

Este projecto consiste no desenvolvimento de uma aplicação de base de dados distribuída, organizada através de uma rede de nós interligados em anel (*Ring Network*).

A aplicação permite que os nós entrem e saiam da rede, comuniquem entre si e realizem operações de pesquisa distribuída.

O projecto está implementado em C e utiliza comunicação através dos protocolos UDP e TCP.

## Estrutura do Projecto

```text
PL01_RC1/
├── Makefile
├── headers/
│   └── header.h
└── src/
    ├── main.c
    ├── process.c
    ├── bootstrap.c
    ├── join.c
    ├── leave.c
    ├── search.c
    └── server.c
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

## Outros Ficheiros

### `header.h` — Definições e declarações partilhadas

Contém as definições utilizadas pelos diferentes módulos do projecto, incluindo estruturas de dados, constantes, bibliotecas e protótipos de funções.

### `Makefile` — Compilação do projecto

Automatiza o processo de compilação dos ficheiros-fonte e a geração do executável `ddt`.

Disponibiliza os seguintes comandos:

| Comando | Descrição |
|---|---|
| `make` | Compila o projecto |
| `make clean` | Remove os ficheiros objecto |
| `make fclean` | Remove os ficheiros objecto e o executável |
| `make re` | Limpa e recompila o projecto |

## Compilação

Para compilar o projecto, executar na raiz do repositório:

```bash
make
```

Após a compilação, será gerado o executável:

```text
ddt
```
