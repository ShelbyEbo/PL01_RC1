# Plano de Testes — `ddt` (RC-I 2026, Projecto de Laboratório 1)

Este documento lista os testes a fazer à aplicação `ddt` e ao servidor de arranque (`boot_server`), com os comandos exactos. Os testes seguem a ordem da estratégia do enunciado (§3.1): servidor de arranque → nó sozinho → `join` directo → `search` → `join` com pesquisa → `leave` → robustez.

---

## 0. Infraestrutura

| VM      | IP              | Papel                                     | Porta              |
|---------|-----------------|-------------------------------------------|--------------------|
| `boot`  | 192.168.56.21   | `boot_server` (serviço systemd)           | 58000/udp          |
| `node1` | 192.168.56.22   | nó `ddt`                                  | 58001/tcp (sugest.) |
| `node2` | 192.168.56.23   | nó `ddt`                                  | 58002/tcp (sugest.) |
| `node3` | 192.168.56.24   | nó `ddt`                                  | 58003/tcp (sugest.) |

```bash
# No host (pasta do projecto)
vagrant up                                   # cria, instala pacotes, compila, arranca boot_server
vagrant status                               # todas as VMs "running"
vagrant provision --provision-with build     # recompila em todas as VMs depois de mudar o código
vagrant provision boot                       # recompila e reinicia o boot_server

# Abrir um terminal por VM
vagrant ssh boot
vagrant ssh node1    # e depois: cd ~/ddt
vagrant ssh node2
vagrant ssh node3
```

Ver os logs do servidor de arranque em tempo real (deixar aberto durante todos os testes):

```bash
vagrant ssh boot -c 'journalctl -u boot_server -f'
```

Ver o tráfego do protocolo (opcional, muito útil para o relatório):

```bash
# Na VM boot: tráfego UDP do servidor de arranque
sudo tcpdump -i any -A -n udp port 58000
# Num nó: tráfego TCP do anel
sudo tcpdump -i any -A -n 'tcp portrange 58001-58010'
```

> Para testar vários nós sem VMs extra, pode-se correr vários `ddt` na mesma VM com portas diferentes (`-t 58011`, `-t 58012`, …) — o enunciado (§3.2) aceita isto.

---

## 1. Compilação

| # | Teste | Comando | Resultado esperado |
|---|-------|---------|--------------------|
| 1.1 | Compila sem avisos (`-Wall -Wextra -Werror`) | `cd ~/ddt && make re` | Sem warnings/erros, gera `ddt` |
| 1.2 | Compila o servidor de arranque | `make bootserver` | Gera `boot_server` |
| 1.3 | Limpeza | `make clean && ls src/*.o` ; `make fclean && ls ddt` | Sem `.o`; sem `ddt`/`boot_server` |
| 1.4 | Recompilação incremental | `touch src/search.c && make` | Só recompila `search.o` e liga |

---

## 2. Servidor de arranque (UDP) isolado, com `nc`

Correr numa VM nó (ou na própria `boot`). `-w1` faz o `nc` esperar 1 s pela resposta.

| # | Teste | Comando | Resposta esperada |
|---|-------|---------|-------------------|
| 2.1 | Anel inexistente | `echo "BQRY 7" \| nc -u -w1 192.168.56.21 58000` | `EMPTY` |
| 2.2 | Registar nó de arranque | `echo "REG 7 12 192.168.56.22 58001" \| nc -u -w1 192.168.56.21 58000` | `OK` |
| 2.3 | Consultar anel registado | `echo "BQRY 7" \| nc -u -w1 192.168.56.21 58000` | `BRSP 7 12 192.168.56.22 58001` |
| 2.4 | Actualizar nó de arranque | `echo "REG 7 20 192.168.56.23 58002" \| nc -u -w1 192.168.56.21 58000` e repetir 2.3 | `OK`, depois `BRSP 7 20 192.168.56.23 58002` |
| 2.5 | Vários anéis em simultâneo | `REG 8 …` e `BQRY 7` / `BQRY 8` | Cada anel devolve o seu nó |
| 2.6 | Anular anel | `echo "UNR 7" \| nc -u -w1 192.168.56.21 58000` e repetir 2.1 | `OK`, depois `EMPTY` |
| 2.7 | `UNR` de anel inexistente | `echo "UNR 99" \| nc -u -w1 192.168.56.21 58000` | `OK` (ou erro definido pelo grupo) — o servidor não pode morrer |
| 2.8 | Mensagem desconhecida | `echo "OLA" \| nc -u -w1 192.168.56.21 58000` | Sem resposta; log "Comando desconhecido"; servidor continua vivo |
| 2.9 | Mensagens mal formadas | `echo "BQRY" \| nc …` ; `echo "REG 1 2" \| nc …` ; `echo "REG a b c d" \| nc …` | Ignoradas, servidor continua vivo (repetir 2.1 para confirmar) |
| 2.10 | Datagrama grande | `head -c 2000 /dev/zero \| tr '\0' A \| nc -u -w1 192.168.56.21 58000` | Servidor continua vivo |
| 2.11 | Porta personalizada | `~/ddt/boot_server -p 59000` (noutra VM) + `echo "BQRY 1" \| nc -u -w1 <ip> 59000` | `EMPTY` |
| 2.12 | Porta ocupada | Correr `~/ddt/boot_server` na VM `boot` (o serviço já usa 58000) | Erro "bind" e termina com código ≠ 0 |

> Limpar antes de seguir: `echo "UNR 7" | nc -u -w1 192.168.56.21 58000; echo "UNR 8" | nc -u -w1 192.168.56.21 58000`

---

## 3. Invocação do `ddt`

| # | Teste | Comando | Resultado esperado |
|---|-------|---------|--------------------|
| 3.1 | Valores por omissão | `./ddt -t 58001` → `show` | `Bootstrap Server: 192.168.56.21:58000`, `Servidor TCP: 192.168.56.22:58001` |
| 3.2 | Sem `-t` | `./ddt` → `show` | Porta TCP escolhida pelo SO (≠ 0) |
| 3.3 | Servidor de arranque alternativo | `./ddt -t 58001 -i 192.168.56.23 -p 59000` → `show` | Mostra `192.168.56.23:59000` |
| 3.4 | Opção inválida | `./ddt -x` | Mensagem `Uso: …` e código de saída ≠ 0 (`echo $?`) |
| 3.5 | Porta TCP ocupada | Dois `./ddt -t 58001` na mesma VM | O segundo falha com "Falha crítica…" |
| 3.6 | Servidor de arranque inacessível | `./ddt -t 58001 -i 192.168.56.99` → `join 1 10` | Timeout (~3 s) e erro, sem bloquear a aplicação |
| 3.7 | IP de arranque inválido | `./ddt -t 58001 -i abc` → `join 1 10` | Erro "IP inválido", aplicação continua |
| 3.8 | Verificar servidor TCP a escutar | Noutro terminal da mesma VM: `ss -ltnp \| grep 58001` | Socket em LISTEN |

---

## 4. Interface de utilizador — nó fora do anel

Num nó com `./ddt -t 58001`:

| # | Comando | Resultado esperado |
|---|---------|--------------------|
| 4.1 | `show` | "O nó não pertence a nenhum anel." |
| 4.2 | `leave` | Erro: não pertence a nenhum anel |
| 4.3 | `search 5` | Erro: não pertence a nenhum anel |
| 4.4 | `join` / `join 1` / `join a b` | Mensagem `Uso: join …` |
| 4.5 | `search` / `search x` | Mensagem `Uso: search <id>` |
| 4.6 | `xpto` | `Unknown command: xpto` |
| 4.7 | Linha vazia (Enter) | Nada acontece |
| 4.8 | `exit` | Aplicação termina com código 0 |
| 4.9 | `Ctrl-D` (EOF no stdin) | Aplicação termina sem ciclo infinito |

---

## 5. Nó único no anel (estratégia, passo 2)

`node1`: `./ddt -t 58001`

| # | Comando | Resultado esperado | Verificação no servidor de arranque |
|---|---------|--------------------|-----------------------------------|
| 5.1 | `join 1 10` | "Anel 1 criado. Nó 10 registado…" | Log `BQRY 1 -> EMPTY`, `REG ring=1 node=10 (192.168.56.22:58001)` |
| 5.2 | `show` | Ring 1, Node 10 (Bootstrap), pred = succ = 10 | — |
| 5.3 | `search 0`, `search 10`, `search 63` | Sempre `(10, 192.168.56.22, 58001)` | — |
| 5.4 | `search 64`, `search -1` | Erro: id entre 0 e 63 | — |
| 5.5 | `join 1 20` (já está no anel) | Erro: o nó já pertence ao anel 1 | — |
| 5.6 | `leave` | "Anel 1 anulado…", "Nó abandonou o anel." | Log `UNR ring=1 -> OK`; `echo "BQRY 1" \| nc -u -w1 192.168.56.21 58000` → `EMPTY` |
| 5.7 | `join 1 10` de novo | Volta a criar o anel | `REG` de novo |
| 5.8 | `exit` sozinho no anel | Aplicação termina | `UNR ring=1` no log; `BQRY 1` → `EMPTY` |
| 5.9 | Ids nos limites: `join 2 0`, depois `leave`, `join 2 63` | Ambos aceites | — |
| 5.10 | Id inválido: `join 3 64`, `join 3 -1` | Erro: identificador entre 0 e 63 | — |

---

## 6. `join` directo: `join x i succ succ.IP succ.TCP` (estratégia, passo 3)

Constrói o anel sem pesquisa. Só `node1` regista-se no servidor de arranque.

```text
node1$ ./ddt -t 58001      > join 1 10
node2$ ./ddt -t 58002      > join 1 30 10 192.168.56.22 58001
node3$ ./ddt -t 58003      > join 1 50 10 192.168.56.22 58001     # 50 fica entre 30 e 10
```

| # | Verificação | Esperado |
|---|-------------|----------|
| 6.1 | `show` em node2 após entrar | pred 10, succ 10 |
| 6.2 | `show` em node1 | pred 30, succ 30 |
| 6.3 | `show` nos três após node3 | node1: pred 50 / succ 30 · node2: pred 10 / succ 50 · node3: pred 30 / succ 10 |
| 6.4 | Servidor de arranque **não** recebe `BQRY`/`REG` de node2/node3 | Log sem novas entradas |
| 6.5 | Sucessor inexistente: `join 1 40 99 192.168.56.22 58001` | Erro (id fora de 0..63) |
| 6.6 | Sucessor inacessível: `join 1 40 10 192.168.56.22 58999` | Erro de ligação TCP, nó continua fora do anel |
| 6.7 | Sucessor = próprio id: `join 1 40 40 192.168.56.22 58001` | Erro |
| 6.8 | Mensagem enviada ao sucessor (com `nc` a fazer de nó): `nc -l 58009` noutra VM e `join 1 40 45 <ip_nc> 58009` | O `nc` recebe exactamente `NEW 40 <ip> <porta>\n` |


---

## 7. Pesquisa `search k` (estratégia, passo 4)

Com o anel `10 → 30 → 50 → 10` (node1=10, node2=30, node3=50). Responsável por `k` = primeiro nó no sentido horário com id ≥ k (intervalo `(pred, i]`).

| # | Em | Comando | Resposta esperada | Percurso esperado (tcpdump) |
|---|----|---------|-------------------|------------------------------|
| 7.1 | node1 (10) | `search 10` | `(10, 192.168.56.22, 58001)` — local, sem mensagens | nenhum |
| 7.2 | node1 (10) | `search 5` | `(10, …)` — 5 ∈ (50, 10] com wrap-around | nenhum |
| 7.3 | node1 (10) | `search 20` | `(30, 192.168.56.23, 58002)` | `QRY 10 20` 10→30; `RSP 10 20 30 …` 30→10 |
| 7.4 | node1 (10) | `search 40` | `(50, 192.168.56.24, 58003)` | `QRY 10 40` 10→30→50; `RSP …` 50→30→10 |
| 7.5 | node2 (30) | `search 63` | `(10, …)` — wrap-around | `QRY 30 63` 30→50→10; `RSP` 10→50→30 |
| 7.6 | node3 (50) | `search 0` | `(10, …)` | `QRY 50 0` 50→10; `RSP` 10→50 |
| 7.7 | qualquer | `search 30` / `search 31` | Fronteiras: 30 → nó 30; 31 → nó 50 | — |
| 7.8 | node1 | `search 11` | `(30, …)` (k = pred+1 do nó 30) | — |
| 7.9 | node1 | Várias seguidas: `search 20` `search 40` `search 60` | Três respostas correctas | — |
| 7.10 | todos | Pesquisas simultâneas nos três nós | Cada nó recebe a sua resposta (os `RSP` intermédios não são mostrados nos nós de passagem) | — |

**Nó falso com telnet** (verifica o parsing de `QRY`/`RSP`). Ligar a um nó e fingir ser o predecessor não é trivial, por isso o teste mais simples é usar `ID`, que desencadeia uma pesquisa:

```bash
telnet 192.168.56.22 58001
ID 40                # o nó 10 pesquisa 40 pelo anel
# esperado: SUCC 50 192.168.56.24 58003
```

---

## 8. `join x i` com pesquisa (estratégia, passo 5)

Começar com o servidor de arranque limpo (`echo "UNR 1" | nc -u -w1 192.168.56.21 58000`).

| # | Em | Comando | Esperado |
|---|----|---------|----------|
| 8.1 | node1 | `join 1 10` | Cria o anel (EMPTY → REG) |
| 8.2 | node2 | `join 1 30` | `BRSP 1 10 …`, recebe `SUCC 10 …`, entra; node1: "Novo nó 30 … predecessor e sucessor" |
| 8.3 | node1, node2 | `show` | 10: pred 30 / succ 30 · 30: pred 10 / succ 10 |
| 8.4 | node3 | `join 1 50` | Recebe `SUCC 10 …` (50 ∈ (30,10]) → liga a 10; 10 envia `CON 50 …` a 30; 30 envia `NEW 30 …` a 50 |
| 8.5 | todos | `show` | Anel 10 → 30 → 50 → 10 (igual a 6.3) |
| 8.6 | todos | `search` da secção 7 | Mesmos resultados da tabela 7 |
| 8.7 | node4 (ou 2.º ddt na node1 com `-t 58011`) | `join 1 20` | Entra entre 10 e 30: `SUCC 30 …`; 30 avisa 10 com `CON 20 …` |
| 8.8 | — | `join 1 10` noutro nó (id = nó de arranque) | Erro "já está em uso como nó de arranque" |
| 8.9 | — | `join 1 30` noutro nó (id já usado, não-arranque) | `SUCC 30 …` → erro "identificador já está em uso" |
| 8.10 | — | Nó de arranque morto (`kill -9` ao ddt do node1) e `join 1 40` noutro nó | Erro de ligação ao nó de arranque, sem bloquear |
| 8.11 | — | Duas adesões quase simultâneas a anéis diferentes / ao mesmo anel | Ambas terminam com anel consistente (`show` em todos) |
| 8.12 | — | Dois anéis independentes: nós A,B em `join 1 …` e C,D em `join 2 …` | `search` num anel nunca chega ao outro; `BQRY 1` e `BQRY 2` devolvem nós diferentes |

**Simular o pedido de adesão à mão** (útil para isolar o nó de arranque):

```bash
telnet 192.168.56.22 58001     # ligação ao nó de arranque
ID 25                          # esperado: SUCC 30 192.168.56.23 58002
```

---

## 9. `leave` (estratégia, passo 6)

Anel inicial `10 (arranque) → 30 → 50`. Após cada `leave`, fazer `show` em todos os nós restantes e `echo "BQRY 1" | nc -u -w1 192.168.56.21 58000`.

| # | Quem sai | Mensagens esperadas | Estado final |
|---|----------|---------------------|--------------|
| 9.1 | 30 (nó normal) | 30 → 10: `CON 50 …`; 10 → 50: `NEW 10 …` | 10 ↔ 50; `BQRY 1` → nó 10 |
| 9.2 | 10 (nó de arranque) com 3 nós | `REG 1 30 …` ao servidor; `BOOT` a 30; `CON 30 …` a 50; 50 → 30: `NEW 50 …` | 30 (arranque) ↔ 50; `BQRY 1` → `BRSP 1 30 …`; `show` em 30 indica "(Bootstrap Node)" |
| 9.3 | Um de dois nós (não arranque) | `CON <outro> …` ao outro, que recebe o seu próprio id | O que fica: pred = succ = ele próprio, "Fiquei sozinho" |
| 9.4 | Nó de arranque de dois nós | `REG` + `BOOT` + `CON` | O outro fica sozinho e é o nó de arranque |
| 9.5 | Último nó | `UNR 1` | `BQRY 1` → `EMPTY` |
| 9.6 | Re-entrada | Depois de 9.1, `join 1 30` no mesmo processo | Entra de novo correctamente |
| 9.7 | `search` depois de saídas | Repetir tabela 7 com o anel actual | Respostas consistentes com o novo anel |

---

## 10. Robustez (enunciado §3.1: "terminar graciosamente")

| # | Teste | Comando | Esperado |
|---|-------|---------|----------|
| 10.1 | Mensagem desconhecida por TCP | `telnet 192.168.56.22 58001` → `OLA` | Ligação fechada pelo nó; nó continua a funcionar (`show`) |
| 10.2 | Mensagens mal formadas | `telnet …` → `QRY abc`, `RSP 1 2`, `NEW x`, `ID` | Ligação fechada; nó vivo |
| 10.3 | Linha demasiado longa (> 256) | `python3 -c "print('A'*1000)" \| nc -q1 192.168.56.22 58001` | Ligação fechada; nó vivo |
| 10.4 | Ligação aberta e fechada sem enviar nada | `nc -z 192.168.56.22 58001` | Nó vivo |
| 10.5 | Ligação TCP pendurada | `telnet 192.168.56.22 58001` (sem escrever) + em paralelo `join` de outro nó | A adesão não deve ficar bloqueada indefinidamente |
| 10.6 | Vizinho morre abruptamente | Anel de 3; `kill -9 <pid do ddt de 30>` (`pgrep ddt`) | 10 e 50 detectam o fecho, não crasham; `show` mostra o estado; `search` não bloqueia |
| 10.7 | Escrever para um vizinho morto | Após 10.6, `search 40` em 10 | Erro tratado — **não** pode morrer com SIGPIPE (`echo $?` ≠ 141) |
| 10.8 | `exit` com vizinhos | Anel de 3; `exit` em 30 | Os vizinhos continuam vivos (idealmente `exit` faz `leave` primeiro) |
| 10.9 | Servidor de arranque em baixo durante o `leave` do nó de arranque | `vagrant ssh boot -c 'sudo systemctl stop boot_server'`; `leave` em 10 | Timeout e aviso, mas o anel é reparado na mesma; depois `sudo systemctl start boot_server` |
| 10.10 | Mensagens partidas em vários segmentos TCP | `(printf 'ID '; sleep 1; printf '40\n'; sleep 3) \| nc 192.168.56.22 58001` | Resposta `SUCC …` igual a enviar a linha inteira |
| 10.11 | Várias mensagens num só segmento | `printf 'ID 40\nID 20\n' \| nc -q3 192.168.56.22 58001` | Duas respostas `SUCC` (ou comportamento definido pelo grupo) |
| 10.12 | CRLF (telnet envia `\r\n`) | `telnet` → `ID 40` | Aceite como `ID 40` |

---

## 11. Memória e depuração

```bash
# Fugas de memória / acessos inválidos num cenário completo (join, search, leave, exit)
valgrind --leak-check=full --show-leak-kinds=all --track-fds=yes ./ddt -t 58001
valgrind --leak-check=full ~/ddt/boot_server -p 59000     # Ctrl-C ao fim

# Depurar um nó
gdb --args ./ddt -t 58001
(gdb) break handle_new
(gdb) run
```

Esperado: `ERROR SUMMARY: 0 errors`; no fim só ficam abertos os descritores 0/1/2 (`--track-fds`).

---

## 12. Problemas encontrados e corrigidos

Estes problemas foram encontrados ao preparar este documento e já estão corrigidos. Os testes indicados servem de teste de regressão.

| # | Problema | Correcção | Testes |
|---|----------|-----------|--------|
| 1 | `handle_new` chamava `conn_drain` (com `recv()` bloqueante) depois do `NEW` → o nó congelava e o 3.º `join` falhava | Nova função `conn_process_buffer()` (`src/server.c`) que só processa linhas já no buffer; `conn_drain` = `recv` + `conn_process_buffer` | 6.3, 8.4, 8.5 |
| 2 | `join x i succ IP TCP` caía sempre no `join` normal (`sscanf` com 3 campos) | `src/process.c` distingue 3 e 6 argumentos; 6 → `join_direct` | 6.x |
| 3 | SIGPIPE matava o processo ao escrever para um vizinho que fechou | `signal(SIGPIPE, SIG_IGN)` em `src/main.c`; o erro chega como `EPIPE` ao `send_line` | 10.6, 10.7 |
| 4 | Só existia `temp[0]`: uma nova ligação fechava a anterior | Usa as `MAX_TEMP` ligações temporárias (accept procura slot livre; o `select` vigia todas) | 8.11, 10.5 |
| 5 | `exit` num anel com vários nós não avisava os vizinhos | `exit` faz `leave` primeiro (CON / REG / BOOT / UNR conforme o caso) | 10.8, 5.8 |
| 6 | `join 1 64` / `join 1 -1` eram aceites | Validação do id (0..63) e do anel (≥ 0) em `src/process.c` | 5.10 |

> Fora do âmbito do enunciado: se um nó morre abruptamente (`kill -9`), o anel **não** é reparado e o servidor de arranque pode ficar com um nó de arranque morto. Os vizinhos detectam o fecho e continuam vivos, mas o anel fica partido até ser reconstruído.

---

## 13. Checklist final (antes da entrega)

- [ ] `vagrant destroy -f && vagrant up` a partir de um clone limpo do repositório funciona sem intervenção.
- [ ] `make re` sem avisos em todas as VMs.
- [ ] Secções 2, 5, 7, 8, 9 passam com 3 VMs + pelo menos 4 nós (usar um 2.º `ddt` numa VM).
- [ ] Secção 10 sem crashes.
- [ ] `valgrind` sem erros.
- [ ] Capturas de `tcpdump` de um `join` e de um `leave` guardadas para o relatório.
