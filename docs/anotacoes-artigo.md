# Anotações para o artigo: segunda entrega

Este arquivo junta, etapa por etapa, o material para a **seção de Desenvolvimento** do artigo.
Ele é atualizado ao fim de cada etapa e será usado para escrever o artigo depois da Etapa 5.

O que as orientações da segunda entrega pedem na seção de Desenvolvimento:
etapas já executadas; implementação realizada; tecnologias e ferramentas; decisões técnicas;
funcionamento das partes implementadas; dificuldades e soluções; figuras com legenda e explicação no texto.

---

## Ajustes pendentes no texto já escrito (proposta)

- [ ] Em "1.2 Problema de pesquisa" e "1.3.1 Objetivo geral", trocar **"linguagem C"** por **"linguagem C++"**
  (decisão da Etapa 1).
- [ ] Objetivos específicos: citar os três modos da FTL comparados (direto, mapeamento sem WL, mapeamento com WL)
  e a amplificação de escrita como métrica extra, se o grupo quiser.

---

## Etapa 0: Preparação do ambiente (concluída)

**O que foi feito**
- Repositório no GitHub (`eduardoBoelter/Projeto-Integrador-IV`), com branches por integrante (ex.: `Eduardo`) e integração na `main`.
- Build com `Makefile` (`make`, `make run`, `make clean`) e `.gitignore` para não versionar binários.

**Tecnologias e ferramentas**
- C++17 com o compilador g++ (MinGW no Windows), Make, Git, GitHub e SourceTree.

**Dificuldades e soluções**
- Executáveis (`main.exe`, `main.o`) tinham sido commitados e só funcionavam no Windows.
  *Solução:* removidos do repositório e ignorados pelo `.gitignore`; o build passou a ser feito pelo `Makefile`.
- Arquivos salvos em Latin-1 apareciam com acentos quebrados no GitHub. *Solução:* padronização em UTF-8.

## Etapa 1: Requisitos e modelagem (concluída)

**O que foi feito**
- Levantamento de 25 requisitos funcionais e 7 não funcionais ([01-requisitos.md](01-requisitos.md)).
- Definição da arquitetura em cinco camadas, das interfaces entre elas e das políticas de GC e wear leveling
  ([02-arquitetura.md](02-arquitetura.md)).

**Decisões técnicas** (boas para o texto, cada uma com o porquê)
1. **C++17 em vez de C.** A camada física já estava escrita em C++; classes e `std::vector` deixam as camadas
   mais organizadas sem perder o controle de baixo nível (acesso byte a byte ao arquivo binário). Sem bibliotecas externas.
2. **Parâmetros:** página de 2 KB, 64 páginas por bloco, 128 blocos (16 MB), limite de 1000 ciclos P/E.
   O limite foi reduzido em relação a chips reais (3 mil a 100 mil ciclos) para a falha ocorrer em tempo viável de simulação.
3. **Over-provisioning de 8 blocos (6,25%)**: a capacidade lógica é de 7680 LBAs (15 MB) nos três modos,
   garantindo uma comparação justa.
4. **1 LBA = 1 página (2 KB)**, simplificando a tradução de endereços.
5. **Três modos de FTL** atrás de uma mesma interface (`FTL`): direto (LBA = PBA), mapeamento sem WL e mapeamento com WL.
   O terceiro modo intermediário permite separar o ganho do mapeamento do ganho do wear leveling.
6. **Garbage collection guloso** (vítima = bloco com mais páginas inválidas), disparado com menos de 2 blocos livres.
7. **Metadados persistidos em `nand_meta.bin`**, separado dos dados, para o desgaste sobreviver entre execuções.
8. **Simplificações:** sem erros de bit nem ECC; o único desgaste modelado é o limite de ciclos P/E.

**Figuras sugeridas** (todas em [02-arquitetura.md](02-arquitetura.md), exportáveis pelo mermaid.live)
- *Figura X: Arquitetura em camadas do simulador.* Explicar que cada camada só acessa a de baixo,
  o que permite trocar o modo da FTL sem alterar o sistema de arquivos.
- *Figura Y: Fluxo de escrita na FTL com mapeamento de páginas.* Explicar a escrita fora do lugar
  (out-of-place), a invalidação da página antiga e quando o GC é acionado.
- *Figura Z: Estados de uma página* (livre, válida, inválida). Mostra a regra de que só o apagamento do bloco
  devolve a página ao estado livre.
- *Tabela: os três modos da FTL* (seção 4 da arquitetura).

**Dificuldades e soluções**
- Divergência entre a proposta (C) e o código (C++). *Solução:* decisão do grupo por manter C++ e ajustar o texto.
- Com chips reais (dezenas de milhares de ciclos), simular até a falha levaria muito tempo.
  *Solução:* limite de 1000 ciclos, configurável por constante.

## Etapa 2: Camada física da NAND (concluída)

**O que foi feito**
- Classe `NANDFlashMemory` (`NANDFlash.hpp`) simula o chip: 128 blocos × 64 páginas × 2 KB = 16 MB,
  guardados no arquivo `nand_device.bin`, criado com todos os bytes em `0xFF` (estado apagado da Flash).
- Cada bloco tem contador de ciclos P/E e marca de bad block; cada página tem estado (livre, válida ou inválida) e LBA.
- `erase_block()` grava `0xFF` no trecho do bloco dentro do arquivo, incrementa o contador P/E e, no ciclo 1000,
  marca o bloco como bad block. O retorno diferencia os casos: `OK`, `WORN_OUT` (morreu agora),
  `BAD_BLOCK` (já estava morto, nada foi feito) e `IO_ERROR`.
- **Persistência do desgaste:** os metadados são gravados em `nand_meta.bin` (`save_metadata()`) e recarregados
  ao abrir o dispositivo (`open()`). Antes, o desgaste se perdia ao fechar o programa.
- `format()` devolve o dispositivo ao estado de fábrica (dados em 0xFF e contadores zerados).
- Programa de teste da camada (`tests/test_nand.cpp`, executado com `make test`): 25 verificações, todas passando.

**Funcionamento (para descrever no texto)**
- Endereço de uma página no arquivo: `(bloco × 64 + página) × 2048` bytes.
- Formato do `nand_meta.bin` (41.628 bytes): cabeçalho com assinatura `NANDMETA`, versão e geometria
  (tamanho de página, páginas por bloco, número de blocos e limite P/E), seguido, para cada bloco, do contador P/E
  (4 bytes), da marca de bad block (1 byte) e, para cada página, do estado (1 byte) e do LBA (4 bytes).
- Ao abrir, o simulador confere a assinatura, a versão, a geometria e o tamanho do arquivo de dados.
  Se algo não bate, o carregamento é recusado sem alterar o estado em memória.

**Decisões técnicas**
1. **Dados e metadados em arquivos separados.** O `nand_device.bin` representa só o conteúdo das células;
   os metadados são informação de controle, que num chip real fica na área reservada (*spare area*) de cada página.
   Separar facilita inspecionar os dois e manter o arquivo de dados com o tamanho exato do dispositivo.
2. **Salvamento explícito dos metadados**, como um *flush*, e não a cada apagamento. Gravar 41 KB a cada operação
   multiplicaria o tempo das simulações longas (dezenas de milhares de apagamentos).
3. **Inteiros gravados em little-endian byte a byte**, para o mesmo `nand_meta.bin` funcionar no Windows e no Linux.
4. **Geometria gravada no cabeçalho**, para impedir que um arquivo criado com outros parâmetros seja carregado por engano.
5. **Contadores de páginas (válidas, inválidas e livres) não são gravados**: são recalculados a partir dos estados,
   evitando inconsistência entre os dois.
6. **Testes automatizados próprios, sem biblioteca externa** (macro `CHECK`), seguindo o requisito de usar só a biblioteca padrão.

**Figuras e evidências sugeridas**
- *Tela: saída do `make test`* com as 25 verificações passando. Explicar que o teste cobre formatação, apagamento,
  fim de vida do bloco, persistência e arquivos inválidos.
- *Tela: o simulador executado duas vezes seguidas*, com o contador P/E do bloco 0 indo de 1 para 2.
  Mostra que o desgaste sobrevive entre execuções.
- *Trecho de código: `erase_block()` e `reset_after_erase()`*, mostrando o incremento do P/E e a marcação de bad block.
- *Figura: layout dos arquivos*: `nand_device.bin` dividido em blocos e páginas, e a estrutura do `nand_meta.bin`.

**Dificuldades e soluções**
- Na primeira versão, cada página guardava uma cópia de 2 KB dos dados em memória, duplicando o arquivo binário.
  *Solução:* a memória passou a guardar só os metadados; o conteúdo fica apenas no arquivo.
- O apagamento retornava `false` tanto quando o bloco morria naquele ciclo quanto quando já estava morto.
  *Solução:* o enum `EraseResult`, que diferencia os casos.
- O desgaste se perdia a cada execução, o que impediria simulações em várias sessões.
  *Solução:* o arquivo `nand_meta.bin` com validação de formato e geometria.

## Etapa 3: Driver de E/S

*(a preencher)*

## Etapa 4: FTL com mapeamento direto

*(a preencher)*

## Etapa 5: FTL com mapeamento e garbage collection

*(a preencher)*
