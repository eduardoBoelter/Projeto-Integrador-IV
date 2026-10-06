# Etapa 1: Requisitos do simulador

Este documento fixa **o que** o simulador precisa fazer, com quais parâmetros e com quais
simplificações. A arquitetura (o **como**) está em [02-arquitetura.md](02-arquitetura.md).

## 1. Escopo

O simulador modela um pendrive que funciona como dispositivo de bloco. Ele é formado por
cinco partes, de baixo para cima:

1. **Camada física NAND**, guardando o conteúdo num arquivo binário (`nand_device.bin`).
2. **Driver de E/S**, com leitura, programação (escrita) e apagamento.
3. **FTL** (Flash Translation Layer), com três modos de operação para comparação.
4. **Sistema de arquivos mínimo**, que gera a carga de escrita.
5. **Shell interativo**, para operar e demonstrar o simulador.

## 2. Parâmetros da simulação

| Parâmetro | Valor | Justificativa |
|---|---|---|
| Tamanho da página | 2 KB (2048 bytes) | Menor valor típico citado no referencial (2 KB a 8 KB) |
| Páginas por bloco | 64 | Valor típico (64 a 128 páginas por bloco) |
| Total de blocos | 128 | Capacidade física de 16 MB: pequena o bastante para simular rápido |
| Limite de ciclos P/E | 1000 | Reduzido em relação a chips reais (3 mil a 100 mil) para a falha acontecer em segundos de simulação |
| Blocos reservados (over-provisioning) | 8 (6,25%) | Espaço extra que a FTL precisa para redirecionar escritas e fazer garbage collection |
| Capacidade lógica | 120 blocos × 64 páginas = **7680 LBAs** (15 MB) | Mesma capacidade nos três modos, para a comparação ser justa |
| Tamanho do setor lógico (LBA) | 2 KB, igual à página | Simplificação: 1 LBA ocupa exatamente 1 página |

Todos os valores ficam em constantes no código e podem ser alterados para novos experimentos.

## 3. Requisitos funcionais

### Camada física (Etapa 2)
- **RF01.** Armazenar o conteúdo das páginas no arquivo `nand_device.bin`, criado com todos os bytes em `0xFF` (estado apagado).
- **RF02.** Manter, para cada página, o estado (livre, válida ou inválida) e o LBA associado.
- **RF03.** Manter, para cada bloco, o contador de ciclos P/E e a marcação de bad block.
- **RF04.** Marcar o bloco como bad block quando o contador atingir o limite de ciclos P/E.
- **RF05.** Salvar e recarregar os metadados (contadores e estados) num segundo arquivo, `nand_meta.bin`, para o desgaste não se perder entre execuções.

### Driver de E/S (Etapa 3)
- **RF06.** Ler uma página (`read_page`).
- **RF07.** Programar uma página (`program_page`) somente se ela estiver livre. Regravar uma página já escrita é proibido, como na Flash real.
- **RF08.** Apagar um bloco inteiro (`erase_block`), recusando bad blocks.
- **RF09.** Atualizar os contadores de páginas livres, válidas e inválidas de cada bloco.
- **RF10.** Contar o total de leituras, programações e apagamentos físicos.

### FTL (Etapas 4, 5 e 6)
- **RF11.** Oferecer à camada de cima apenas `read(lba)` e `write(lba)`, escondendo blocos e páginas.
- **RF12. Modo direto (LBA = PBA):** o LBA sempre aponta para a mesma página física. Alterar um dado exige ler o bloco, apagá-lo e regravá-lo inteiro.
- **RF13. Modo com mapeamento:** manter a tabela LBA → (bloco, página); toda alteração vai para uma página livre e a antiga vira inválida.
- **RF14. Garbage collection:** quando restarem poucos blocos livres, escolher o bloco com mais páginas inválidas, copiar as válidas e apagá-lo.
- **RF15. Wear leveling dinâmico:** ao escolher um bloco livre, preferir o de menor contador de P/E.
- **RF16. Wear leveling estático (opcional):** quando a diferença entre o bloco mais e o menos desgastado passar de um limite, mover os dados frios para um bloco desgastado.
- **RF17.** Retirar de uso os bad blocks; declarar fim de vida do dispositivo quando não houver blocos livres suficientes.

### Sistema de arquivos mínimo (Etapa 7)
- **RF18.** Ter uma área de metadados em LBAs fixos (superbloco, tabela de alocação e diretório) e uma área de dados.
- **RF19.** Criar, escrever, ler, listar e apagar arquivos.
- **RF20.** Cada operação deve atualizar os metadados, concentrando escritas nos mesmos LBAs (dados quentes).

### Shell (Etapa 8)
- **RF21.** Comandos para formatar, escolher o modo da FTL, operar arquivos, rodar cenários de carga e exibir estatísticas.

### Métricas e testes (Etapas 9 e 10)
- **RF22.** Calcular média e **desvio padrão** dos contadores de apagamento por bloco.
- **RF23.** Registrar o **momento da primeira falha de bloco**, medido em número de escritas lógicas.
- **RF24.** Calcular a amplificação de escrita (páginas gravadas fisicamente ÷ páginas pedidas pelo sistema de arquivos).
- **RF25.** Exportar os resultados em CSV para gerar tabelas e gráficos.

## 4. Requisitos não funcionais

- **RNF01. Linguagem:** C++17, sem bibliotecas externas (apenas a biblioteca padrão). Decisão confirmada em 06/10/2026: o texto do artigo deve trocar "linguagem C" por "C++".
- **RNF02. Portabilidade:** compilar com g++ no Linux e com MinGW no Windows.
- **RNF03. Reprodutibilidade:** as cargas aleatórias usam semente fixa, para o mesmo teste dar o mesmo resultado.
- **RNF04. Desempenho:** um cenário completo, até a primeira falha, deve rodar em poucos minutos num computador comum.
- **RNF05. Separação em camadas:** cada camada só conversa com a camada imediatamente abaixo.
- **RNF06. Testabilidade:** cada camada tem um programa de teste próprio (`make test`).
- **RNF07. Código comentado em português**, para facilitar a escrita do artigo.

## 5. Simplificações assumidas

- Não há erros de bit, ECC nem falhas aleatórias: o único desgaste modelado é o limite de ciclos P/E.
- Uma página só tem dados ou está apagada; não simulamos tempos de leitura e escrita reais.
- O bloco vira bad block exatamente no ciclo de número `MAX_PE_CYCLES`, sem variação entre blocos.
- Os metadados da FTL ficam em memória e em `nand_meta.bin`, e não dentro da própria NAND.
