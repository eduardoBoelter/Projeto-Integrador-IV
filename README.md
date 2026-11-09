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

`./simulador` abre o dispositivo existente (`nand_device.bin` com os dados e
`nand_meta.bin` com o desgaste), ou formata um novo se não houver, e demonstra a FTL com
mapeamento de páginas: cada alteração de um LBA vai para uma página nova.
`./simulador --format` força a formatação e zera o desgaste.

`./simulador --cenario-quente` roda o cenário de dados quentes (90% das escritas nos
metadados) até a primeira falha de bloco no mapeamento direto e no mapeamento de páginas,
mostra a tabela comparativa e exporta o desgaste por bloco em `desgaste_direto.csv` e
`desgaste_mapeamento.csv`. Usa arquivos próprios e não altera o dispositivo acima.

## Documentação

- [Requisitos](docs/01-requisitos.md)
- [Arquitetura](docs/02-arquitetura.md)
- [Anotações para o artigo](docs/anotacoes-artigo.md)
