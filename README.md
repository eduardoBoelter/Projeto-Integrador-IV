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
| `exportar <arquivo.csv>` | grava os ciclos P/E dos 128 blocos em CSV |
| `comparar` | roda os experimentos nos quatro modos e abre a tela de resultados |
| `ajuda`, `sair` | lista os comandos / salva e encerra |

Os comandos também podem vir de um arquivo: `./simulador < comandos.txt`.

`./simulador --demo` mostra a FTL com mapeamento gravando o mesmo LBA em páginas diferentes.

### Resultados na tela

`./simulador --comparar` roda as duas cargas até a primeira falha de bloco nos quatro modos
da FTL (direto, mapeamento sem wear leveling, wear leveling dinâmico e dinâmico + estático),
mostra as tabelas no terminal e abre no navegador a **tela de resultados** (`resultados.html`):

- quantas escritas cada modo aguentou até o primeiro bloco falhar e o ganho do wear leveling;
- quanto da vida total dos blocos foi aproveitada;
- o mapa de desgaste dos 128 blocos e o histograma dos ciclos P/E;
- a evolução do desgaste (bloco mais gasto e média) até a falha;
- a tabela com todas as métricas.

A página é um arquivo só, sem dependências: abre em qualquer navegador, inclusive sem
internet. Se o navegador não abrir sozinho, dê dois cliques em `resultados.html`. Um exemplo
já gerado está em [docs/resultados.html](docs/resultados.html). No shell, o comando
`comparar` faz o mesmo.

As cargas são:

- **uso de pendrive** (`--cenario-fs`): fotos e músicas que não mudam ocupam 60% do disco e
  documentos são editados, criados e apagados o tempo todo pelo sistema de arquivos;
- **dados quentes** (`--cenario-quente`): disco cheio e 90% das escritas nos LBAs 0 a 3.

Os resultados também são exportados em CSV com o prefixo do cenário (`fs_` ou `quente_`):
`resumo.csv` (todas as métricas por modo), `desgaste_por_bloco.csv` (ciclos P/E de cada
bloco) e `evolucao_<modo>.csv` (média, desvio e máximo ao longo das escritas). Os
experimentos usam arquivos próprios e não alteram o dispositivo do shell. Acrescente
`--sem-abrir` para gravar a página sem abrir o navegador.

## Resultado principal

No uso de pendrive (fotos e músicas que nunca mudam ocupando 60% do disco), o wear leveling
dinâmico + estático aguentou **7.822.722 escritas até o primeiro bloco falhar, 8,9 vezes mais
que o mapeamento sem wear leveling**, aproveitando 96,7% da vida dos blocos com cerca de 1% de
escritas extras. O dinâmico sozinho ganhou só 17% nessa carga. Detalhes em
[docs/resultados.md](docs/resultados.md).

## Documentação

- [Requisitos](docs/01-requisitos.md)
- [Arquitetura](docs/02-arquitetura.md)
- [Resultados e análise](docs/resultados.md)
- [Anotações para o artigo](docs/anotacoes-artigo.md)
