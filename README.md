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
`nand_meta.bin` com o desgaste) ou formata um novo, se não houver.
`./simulador --format` força a formatação e zera o desgaste.

## Documentação

- [Requisitos](docs/01-requisitos.md)
- [Arquitetura](docs/02-arquitetura.md)
- [Anotações para o artigo](docs/anotacoes-artigo.md)
