# Projeto Integrador IV

Simulação de um dispositivo de bloco com gerenciamento de Wear Leveling.

## Compilação

```sh
make        # gera o executável ./simulador
make run    # compila e executa
make test   # compila e executa os testes de cada camada
make clean  # remove binários e os arquivos .bin do dispositivo
```

No Windows (MinGW), use `mingw32-make` ou compile diretamente:
`g++ -std=c++17 -Wall -Wextra main.cpp -o simulador.exe`.

## Execução

`./simulador` abre o **shell interativo** sobre o pendrive simulado (`nand_device.bin` com os
dados, `nand_meta.bin` com o desgaste e `nand_config.txt` com o modo da FTL). Se não houver
dispositivo, cria um novo no modo com wear leveling estático. Comandos principais:

| Comando | O que faz |
|---|---|
| `formatar <modo>` | apaga tudo e zera o desgaste; modos `direto`, `mapeamento`, `dinamico`, `estatico` |
| `ls`, `criar`, `escrever`, `ler`, `apagar` | operações com arquivos |
| `carga <n>` | executa n operações de uso de pendrive (editar, criar e apagar documentos) |
| `estatisticas` | escritas, amplificação, GC, migrações e resumo do desgaste |
| `desgaste` | ciclos P/E dos 128 blocos |
| `ajuda`, `sair` | lista os comandos / salva e encerra |

Os comandos também podem vir de um arquivo: `./simulador < comandos.txt`.

`./simulador --demo` mostra a FTL com mapeamento gravando o mesmo LBA em páginas diferentes.

`./simulador --cenario-quente` roda o cenário de dados quentes (90% das escritas nos
metadados) até a primeira falha de bloco em quatro modos da FTL (direto, mapeamento sem
wear leveling, wear leveling dinâmico e dinâmico + estático), mostra a tabela comparativa e
exporta o desgaste por bloco em `desgaste_<modo>.csv`. Usa arquivos próprios e não altera
o dispositivo do shell.

## Documentação

- [Requisitos](docs/01-requisitos.md)
- [Arquitetura](docs/02-arquitetura.md)
- [Anotações para o artigo](docs/anotacoes-artigo.md)
