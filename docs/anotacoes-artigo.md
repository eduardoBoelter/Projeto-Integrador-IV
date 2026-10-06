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

## Etapa 3: Driver de E/S (concluída)

**O que foi feito**
- Classe `IODriver` (`IODriver.hpp`): a única porta de acesso da FTL à memória. Oferece
  `read_page()`, `program_page()`, `invalidate_page()` e `erase_block()`, todas endereçadas por
  `PhysicalAddress {bloco, página}` (o PBA).
- A camada física ganhou as operações "cruas" `read_page_data()` e `write_page_data()`, que só movem os 2 KB
  de/para o arquivo. As regras da Flash ficam no driver.
- Contadores de operações (`IOStats`): leituras, programações, invalidações, apagamentos e operações recusadas.
  Vão servir de base para a métrica de amplificação de escrita.
- Programa de teste do driver (`tests/test_driver.cpp`): 27 verificações. Com as 25 da camada física,
  o `make test` passa a ter 52 verificações, todas passando.

**Funcionamento (para descrever no texto)**
- **Regras aplicadas pelo driver**, que reproduzem as restrições físicas descritas no referencial teórico:
  1. leitura e programação são feitas por página (2 KB);
  2. uma página só pode ser programada se estiver **livre**: regravar uma página válida ou inválida é recusado
     com `NOT_FREE` (não existe escrita no lugar, *in-place write*);
  3. o apagamento é feito por **bloco inteiro** e é a única forma de devolver as páginas ao estado livre;
  4. bad blocks não aceitam programação nem apagamento (`BAD_BLOCK`);
  5. endereços inexistentes são recusados (`OUT_OF_RANGE`).
- `program_page()` grava os dados e marca a página como **válida**, guardando o LBA que ela contém.
- `invalidate_page()` marca a página como **inválida** sem tocar no arquivo: assim como na Flash real,
  o dado antigo continua lá até o bloco ser apagado. É o que a FTL fará quando um LBA for atualizado em outro lugar.
- Cada operação atualiza os contadores de páginas livres, válidas e inválidas do bloco. O garbage collection
  (Etapa 5) vai usar o contador de inválidas para escolher qual bloco apagar.
- Ciclo de vida de uma página: **livre → (program_page) → válida → (invalidate_page) → inválida → (erase_block) → livre**
  (diagrama de estados na seção 7 da arquitetura).

**Decisões técnicas**
1. **Separar "física" e "driver".** A camada física só sabe mover bytes e contar desgaste; o driver impõe as regras.
   Assim, cada camada é testada isoladamente e a FTL nunca acessa o arquivo diretamente.
2. **Arquivo de dados mantido aberto** durante toda a execução (antes era reaberto a cada apagamento).
   As simulações vão fazer centenas de milhares de operações de página, e reabrir o arquivo em cada uma seria lento.
   Cada escrita é seguida de `flush()`, para o conteúdo no disco estar sempre atualizado.
3. **Retornos com enum (`IOResult`)** em vez de `bool`, para o chamador saber *por que* a operação falhou.
4. **Leitura de página livre é permitida** e retorna `0xFF`, como na Flash real.
5. **Simplificação:** a Flash real exige que as páginas de um bloco sejam programadas em ordem (0, 1, 2...).
   O driver não impõe essa regra; a FTL da Etapa 5 já vai gravar sempre em ordem.

**Figuras e evidências sugeridas**
- *Tela: execução do `./simulador`*, mostrando a gravação, a leitura de volta da mensagem, a tentativa de sobrescrita
  recusada com `NOT_FREE` e a invalidação. Na segunda execução, aparece o apagamento do bloco antes de gravar de novo.
  É uma boa figura para explicar a assimetria "escreve por página, apaga por bloco".
- *Tela: saída do `make test`* com as verificações das duas camadas.
- *Trecho de código: `program_page()`*, mostrando a verificação de página livre e a atualização dos contadores.
- *Tabela: regras do driver e o código de retorno de cada uma* (lista acima).

**Dificuldades e soluções**
- Reabrir o arquivo a cada operação ficaria lento nas simulações. *Solução:* o arquivo fica aberto durante toda
  a execução, com `flush()` após cada escrita.
- Ao manter o arquivo aberto, uma tentativa de abrir um arquivo inválido não pode derrubar o dispositivo em uso.
  *Solução:* `open()` só troca o arquivo depois que toda a validação passa.
- O apagamento de um bloco inexistente lançava exceção. *Solução:* novo retorno `EraseResult::OUT_OF_RANGE`.

## Etapa 4: FTL com mapeamento direto (concluída)

**O que foi feito**
- Interface comum `FTL` (`FTL.hpp`), com `write(lba)` e `read(lba)`. É tudo o que o sistema de arquivos vai enxergar.
  Os três modos da pesquisa vão implementar essa mesma interface.
- Primeiro modo: `DirectFTL`, o **mapeamento direto (LBA = PBA)**, que é o cenário **sem nivelamento** da comparação.
- Capacidade lógica fixada em 7680 LBAs (120 blocos × 64 páginas = 15 MB). Os 8 blocos restantes ficam reservados
  (over-provisioning) e só serão usados pelos modos com mapeamento, mantendo a mesma capacidade visível nos três modos.
- Métricas já registradas pela FTL: escritas lógicas, momento da primeira falha de bloco (número da escrita lógica e qual bloco)
  e amplificação de escrita.
- **Cenário de dados quentes** (`./simulador --cenario-quente`): primeira medição do problema da pesquisa,
  com exportação do desgaste por bloco em CSV.
- Programa de teste da FTL (`tests/test_ftl_direct.cpp`): 20 verificações. O `make test` passa a ter 72, todas passando.

**Funcionamento (para descrever no texto)**
- Tradução de endereço: `bloco = LBA / 64` e `página = LBA % 64`. O LBA 130, por exemplo, fica sempre no bloco 2, página 2.
- Primeira gravação de um LBA: a página está livre e é programada diretamente, sem apagar nada.
- Alteração de um LBA já gravado: como a Flash não permite sobrescrever, a FTL faz o ciclo **read-modify-erase-write**:
  1. lê para a RAM todas as páginas válidas do bloco;
  2. troca, na RAM, a página que mudou;
  3. apaga o bloco inteiro (+1 ciclo P/E);
  4. regrava todas as páginas.
  Ou seja, alterar 2 KB custa o apagamento de um bloco de 128 KB e a regravação de até 64 páginas.
- Quando o bloco chega a 1000 ciclos, ele vira bad block durante o apagamento. No mapeamento direto não existe outro lugar
  para colocar os LBAs dele, então os dados do bloco são perdidos e a escrita retorna `DEVICE_WORN_OUT`.
  Os LBAs dos outros blocos continuam funcionando.

**Resultado do cenário de dados quentes** (semente fixa 2026, resultado reproduzível)
- Fase 1: o disco inteiro é gravado uma vez (7680 escritas, como copiar arquivos para o pendrive).
- Fase 2: 90% das escritas vão para os LBAs 0 a 3 (metadados: superbloco, tabela de alocação e diretório)
  e 10% para LBAs aleatórios de dados.

| Métrica | Valor |
|---|---|
| Primeira falha | escrita lógica nº **8812** (bloco 0), apenas **1132 escritas** depois do preenchimento |
| Apagamentos por bloco | média **8,84**, desvio padrão **87,96**, máximo **1000** |
| Blocos nunca apagados | **42 de 128** |
| Amplificação de escrita na fase 2 | **63,94** (cada escrita lógica regravou, em média, 64 páginas) |
| Blocos mais desgastados | bloco 0: 1000 ciclos; o segundo mais desgastado tem só **5** |

Interpretação para o texto: o dispositivo falhou com **menos de 1% do desgaste total disponível usado**
(1132 apagamentos de um total de 128 × 1000 = 128.000 possíveis). O bloco dos metadados morreu enquanto 42 blocos
nunca foram apagados. É exatamente o problema descrito na introdução da proposta, agora medido no simulador.
O desvio padrão alto (87,96 para uma média de 8,84) é o número que o wear leveling deverá reduzir.

**Decisões técnicas**
1. **Interface abstrata `FTL`** (classe com métodos virtuais): o sistema de arquivos e os cenários de teste recebem
   uma `FTL&` e não sabem qual modo está rodando. Isso garante que a comparação use exatamente a mesma carga.
2. **Mesma capacidade lógica nos três modos** (7680 LBAs), mesmo que o modo direto não use os blocos reservados.
3. **Regravação só das páginas que estavam válidas**: páginas livres do bloco continuam livres após o read-modify-erase-write.
4. **Leitura de LBA nunca gravado retorna 0xFF** (como um setor vazio), em vez de erro.
5. **Desvio padrão calculado sobre os 128 blocos físicos**, inclusive os reservados, para comparar de forma justa
   com os modos que usam todos os blocos.
6. **Semente fixa (2026)** no gerador aleatório do cenário, atendendo ao requisito de reprodutibilidade.

**Figuras e evidências sugeridas**
- *Gráfico de barras: ciclos P/E por bloco no mapeamento direto* (dados em `desgaste_direto.csv`): uma barra de 1000
  no bloco 0 e o resto quase zerado. É a figura mais forte da segunda entrega e será comparada com a do wear leveling na entrega final.
- *Tela: saída do `./simulador --cenario-quente`* com a tabela de resultados.
- *Tela: `./simulador` executado duas vezes*, mostrando o P/E do bloco 0 subindo a cada alteração do LBA 0.
- *Figura: o ciclo read-modify-erase-write* (os 4 passos acima), explicando por que uma alteração pequena custa um bloco inteiro.
- *Trecho de código: `DirectFTL::rewrite_block()`*.

**Dificuldades e soluções**
- A página-alvo de uma regravação podia não estar marcada como válida (por exemplo, invalidada pela demonstração da Etapa 3),
  e então não era regravada. *Solução:* a página-alvo é sempre incluída na regravação com o LBA novo.
- A amplificação de escrita "total" (9,09) escondia o efeito real, porque incluía o preenchimento inicial, em que nenhuma escrita
  apaga nada. *Solução:* o cenário mostra também a amplificação só da fase 2 (63,94).

## Etapa 5: FTL com mapeamento e garbage collection

*(a preencher)*
