# Depurando um crash de *use-after-free* no meniOS

Este guia mostra, passo a passo, como diagnosticar um crash provocado por acesso a memória já liberada (*use-after-free*). O objetivo é destacar o processo e as ferramentas que uso no meniOS, como `nm`, `objdump` e um script em Python com `pyelftools`, para transformar um endereço de falha em uma linha de código. Ao final, você terá um roteiro reaproveitável para incidentes parecidos.

## 1. Preparar o cenário

O binário de exemplo é intencionalmente inseguro: ele libera um ponteiro e logo em seguida tenta escrever naquele endereço.

```c
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    char *ptr = malloc(128);
    if (ptr == NULL) {
        perror("malloc");
        return EXIT_FAILURE;
    }

    free(ptr);
    printf("Dereferencing freed pointer...\n");

    *ptr = 'A';  // Use-after-free: provoca crash
    printf("Value: %c\n", *ptr);

    return EXIT_SUCCESS;
}
```

Compilar com símbolos de depuração deixa o código-fonte acessível na desmontagem e nas tabelas DWARF; esse formato armazena metadados de depuração no ELF, incluindo o mapeamento entre endereços de instruções, arquivos e números de linha, o que é essencial para relacionar endereços a trechos de código.

```bash
gcc -g -O0 -Wall -Wextra use_after_free.c -o use_after_free
```

O meniOS executa binários no formato ELF, sigla para Executable and Linkable Format. Esse contêiner descreve cabeçalhos, seções e tabelas necessárias para carregar o programa na memória, incluindo as tabelas de símbolos que `nm` lê e as seções de código que `objdump` desmonta. Como o ELF também embute as extensões de depuração DWARF geradas pelo compilador, conseguimos cruzar endereços de instruções com arquivos de origem e, a partir disso, chegar rapidamente à linha com defeito.

Enquanto não há suporte a bibliotecas compartilhadas (`.so`), cada aplicativo do meniOS é entregue como um executável ELF independente. Compilo esses binários na máquina host e copio o resultado para a mesma imagem de disco utilizada no boot do sistema, mantendo o fluxo de build simples e previsível.

## 2. Reproduzir o crash e capturar o endereço

Executando o programa no meniOS, ou no ambiente host caso esteja testando localmente, o kernel interrompe o processo assim que detecta o acesso inválido:

```
Dereferencing freed pointer...
  User page fault at 0x0000555555555000 (present=yes write=yes), terminating pid=17
proc_exit: Process init/use_after_free exited with status 11
```

Quando o crash é coletado via console serial do meniOS, o log inclui ainda o contexto de sistema de chamadas. Logo antes da falha, a saída típica de `write(1, …)` registra o ponto para o qual a execução retornaria:

```
syscall_dispatch: pid=17 number=1 rip=40112f cs=3b rsp=bfdfd0
syscall_dispatch: post-handler rip=401168 cs=3b rsp=bfdf10 rax=32
mosh: process terminated by signal 11
```

Anote dois dados essenciais: `rip=0x401168`, que aponta para a instrução que falhou, e o nome do binário (`use_after_free`), porque é nele que vou procurar a origem.

## 3. Descobrir o símbolo com `nm`

Comece localizando qual função cobre o endereço problemático. O `nm` lista a tabela de símbolos do executável, indicando o intervalo de cada função.

```bash
nm -an use_after_free | grep -i main
```

Saída típica:

```
00000000004010f0 t _start
0000000000401120 T main
```

`main` começa em `0x401120`. Como `0x401168` ainda está próximo, é razoável assumir que a falha ocorreu dentro dessa função, mas é prudente confirmar.

## 4. Navegar no assembly com `objdump`

O `objdump` permite correlacionar instruções com o código-fonte. Use as opções `--source` e `--line-numbers` para misturar C e assembly; limite a saída a um trecho curto com `sed`.

```bash
objdump -d --no-show-raw-insn --source --line-numbers use_after_free \
  | sed -n '35,60p'
```

Trecho relevante:

```
use_after_free.c:16
    *ptr = 'A';  // Use-after-free: provoca crash
  401164:  mov    -0x8(%rbp),%rax
  401168:  movb   $0x41,(%rax)          ; 0x41 == 'A'

use_after_free.c:17
    printf("Value: %c\n", *ptr);
```

Isso confirma visualmente: a instrução em `0x401168` é exatamente a escrita com `'A'`. Se o log apontar um endereço alguns bytes adiante, use o mesmo procedimento para encontrar a linha correspondente.

## 5. Resolver linha e arquivo com Python + `pyelftools`

As tabelas DWARF do executável guardam, para cada unidade de compilação, a sequência de endereços e as respectivas linhas do código-fonte. Para automatizar o mapeamento de endereços, um script curto em Python consulta esse material. Esse procedimento economiza tempo quando coleto crashes em lote ou quando quero integrar a análise em pipelines de CI.

```python
#!/usr/bin/env python3

import os
import sys
from elftools.elf.elffile import ELFFile

def lookup(addr, path):
    with open(path, "rb") as f:
        elf = ELFFile(f)
        dwarf = elf.get_dwarf_info()
        for cu in dwarf.iter_CUs():
            lineprog = dwarf.line_program_for_CU(cu)
            prev = None
            for entry in lineprog.get_entries():
                if entry.state is None:
                    continue
                state = entry.state
                if state.end_sequence:
                    prev = None
                    continue
                if prev and prev.address <= addr < state.address:
                    file_entry = lineprog["file_entry"][prev.file - 1]
                    comp_dir_attr = cu.get_top_DIE().attributes.get("DW_AT_comp_dir")
                    comp_dir = ""
                    if comp_dir_attr:
                        comp_dir = comp_dir_attr.value.decode()
                    directory = comp_dir
                    if file_entry.dir_index not in (0, None):
                        include_dirs = lineprog["include_directory"]
                        directory = os.path.join(
                            comp_dir,
                            include_dirs[file_entry.dir_index - 1].decode(),
                        )
                    filename = file_entry.name.decode()
                    full_path = os.path.join(directory, filename) if directory else filename
                    return full_path, prev.line
                prev = state
            if prev and prev.address <= addr:
                file_entry = lineprog["file_entry"][prev.file - 1]
                comp_dir_attr = cu.get_top_DIE().attributes.get("DW_AT_comp_dir")
                comp_dir = comp_dir_attr.value.decode() if comp_dir_attr else ""
                directory = comp_dir
                if file_entry.dir_index not in (0, None):
                    include_dirs = lineprog["include_directory"]
                    directory = os.path.join(
                        comp_dir,
                        include_dirs[file_entry.dir_index - 1].decode(),
                    )
                filename = file_entry.name.decode()
                full_path = os.path.join(directory, filename) if directory else filename
                return full_path, prev.line
    return None, None

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print(f"Uso: {sys.argv[0]} <ELF> <endereco_hex>", file=sys.stderr)
        sys.exit(1)
    binary, raw_addr = sys.argv[1], sys.argv[2]
    address = int(raw_addr, 16)
    src, line = lookup(address, binary)
    if src is None:
        print("Endereço não encontrado.")
        sys.exit(2)
    print(f"{src}:{line}")
```

Execute assim:

```bash
./resolve_addr.py use_after_free 0x401168
```

Saída:

```
/path/para/use_after_free.c:16
```

Integre o script ao seu fluxo de crash dumps no meniOS: basta salvar o endereço de `rip` e, depois, usar a ferramenta para apontar diretamente para a linha culpada.

## 6. Confirmar a correção

Depois de identificar a origem, ajuste o código para evitar o uso após `free`. Um fix simples é atrasar a chamada de `free` ou zerar o ponteiro antes de qualquer acesso:

```c
printf("Dereferencing freed pointer...\n");
ptr[0] = 'A';
printf("Value: %c\n", ptr[0]);
free(ptr);
```

Recompile, execute e confirme que o crash desapareceu. Aproveite para rodar o binário antigo novamente e validar que seu processo de depuração continua funcionando.

## Dicas extras

Garanta que o console serial do meniOS esteja ligado durante os testes. Como o sistema roda hoje em emuladores QEMU ou Bochs, redireciono a UART primária para `com1.log` (ou `com1bochs.log`, no caso do Bochs); esse console funciona como uma janela de logs do kernel, exibindo tudo o que o sistema imprime por `kprintf`, incluindo o endereço de instrução, o PID e a syscall ativa no momento do crash; como cada serviço roda em um executável ELF independente, repita o procedimento com o binário correspondente na imagem do meniOS; se quiser detectar problemas dessa classe automaticamente em paralelo no host, compile versões instrumentadas com AddressSanitizer e execute-as em Linux ou macOS, onde o runtime já está disponível.

Seguindo esses passos você transforma um crash misterioso em uma linha específica do código-fonte; quanto mais desenvolvedores adotarem este fluxo, mais rapidamente esses incidentes serão mapeados e a confiabilidade do sistema será reforçada.
