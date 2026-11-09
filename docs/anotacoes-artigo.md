# Anotações para o artigo

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

## Etapa 5: FTL com mapeamento e garbage collection (concluída)

**O que foi feito**
- Classe `PageMappingFTL` (`FTL.hpp`), segundo modo da pesquisa: **mapeamento de páginas sem wear leveling**.
  Implementa a mesma interface `FTL` do modo direto.
- Tabela de mapeamento LBA → PBA (bloco, página) para os 7680 LBAs.
- Escrita fora do lugar (*out-of-place*): toda gravação vai para a próxima página livre do **bloco ativo**,
  a versão anterior do LBA vira inválida e a tabela passa a apontar para a página nova.
- **Garbage collection guloso**: acionado quando restam menos de 2 blocos livres; copia as páginas válidas
  da vítima e a apaga, devolvendo-a à lista de livres.
- Reconstrução da tabela ao abrir o dispositivo, a partir do estado e do LBA de cada página (salvos em `nand_meta.bin`).
- O cenário de dados quentes agora roda nos dois modos e imprime uma tabela comparativa.
- Programa de teste (`tests/test_ftl_mapping.cpp`): 25 verificações. O `make test` passa a ter **97**, todas passando.

**Funcionamento (para descrever no texto)**
- *Escrita:* `write(lba)` → grava em (bloco ativo, próxima página) → invalida a página antiga → atualiza `tabela[lba]`.
  Alterar um dado **não apaga nada**: o custo do apagamento é adiado para o GC.
- *Bloco ativo cheio:* a FTL pega um novo bloco da lista de livres. Antes disso, se houver menos de 2 livres, roda o GC.
- *Garbage collection (Figura Y da arquitetura):*
  1. escolhe a vítima: o bloco com **mais páginas inválidas** (é o que libera mais espaço copiando menos);
  2. copia as páginas ainda válidas para o bloco ativo, atualizando a tabela;
  3. apaga a vítima (+1 ciclo P/E) e a devolve à lista de livres.
- *Falha de bloco:* se a vítima morre no apagamento, os dados dela **já estão a salvo** em outro bloco.
  O dispositivo perde só capacidade de reserva e continua funcionando, ao contrário do modo direto, que perde os dados.
- *Por que os 8 blocos reservados importam:* com o disco logicamente cheio, ainda existem 8 blocos de folga
  (6,25%) para onde redirecionar as escritas. Sem eles, o GC não teria para onde copiar as páginas válidas.
- *Escolha do bloco livre nesta etapa:* sempre o de **menor número**, sem olhar o desgaste. É a política ingênua
  que a Etapa 6 vai trocar pelo wear leveling (bloco de menor P/E). Só essa função (`select_free_block()`) muda.

**Resultado do cenário de dados quentes** (mesma carga, semente 2026, até a primeira falha de bloco)

| Métrica | Direto (LBA = PBA) | Mapeamento sem WL |
|---|---|---|
| Escritas após o preenchimento até a 1ª falha | 1.132 | **1.345.345** (≈ 1.188 vezes mais) |
| Média de ciclos P/E por bloco | 8,84 | 564,59 |
| Desvio padrão dos ciclos P/E | 87,96 | 163,67 |
| Coeficiente de variação (desvio ÷ média) | **9,95** | **0,29** |
| Blocos nunca apagados | 42 | 0 |
| Amplificação de escrita (fase 2) | 63,94 | 3,40 |
| Execuções do GC / páginas copiadas | — | 72.267 / 3.229.926 |
| Blocos mais desgastados (P/E) | 0 (1000), 6 (5), 101 (5) | 3 (1000), 2 (980), 5 (980), 0 (945), 4 (937) |

Interpretação para o texto:
- Só a troca do mapeamento direto pelo mapeamento de páginas multiplicou a vida útil por cerca de **1.188**,
  porque alterar um dado deixou de custar um apagamento.
- O desvio padrão **absoluto** subiu (de 87,96 para 163,67), mas isso é enganoso: a média também subiu de 8,84 para 564,59.
  Para comparar a **uniformidade** entre modos com médias tão diferentes, o certo é o **coeficiente de variação**,
  que caiu de 9,95 para 0,29. Essa observação é uma boa "decisão técnica" para o artigo.
- Mesmo assim, o desgaste ainda não é uniforme: os blocos de número baixo (0 a 5) estão perto de 1000 ciclos,
  porque a política ingênua sempre reutiliza o bloco livre de menor número. É exatamente o que o wear leveling
  da Etapa 6 deve corrigir, e é a "pergunta em aberto" que a segunda entrega pode deixar para a entrega final.
- A amplificação de escrita de 3,40 mostra o custo do GC: dados quentes e frios misturados nos mesmos blocos
  obrigam o GC a copiar muitas páginas válidas.

**Decisões técnicas**
1. **Mapeamento por página** (e não por bloco): cada LBA pode ir para qualquer página do dispositivo. Usa mais RAM
   (uma entrada por LBA), mas é o que permite a escrita fora do lugar sem apagar nada.
2. **GC guloso** (vítima com mais páginas inválidas), a política clássica descrita por Gal e Toledo (2005).
3. **GC acionado com menos de 2 blocos livres**, garantindo que sempre sobra 1 bloco livre para receber as cópias.
4. **A tabela de mapeamento não é gravada em arquivo**: é reconstruída a partir dos metadados das páginas
   (estado e LBA), como uma FTL real faz ao ler a área reservada (*spare area*) das páginas na inicialização.
5. **Bloco parcialmente gravado volta a ser o bloco ativo** ao reabrir, para não desperdiçar páginas.
6. **Política de alocação isolada numa função virtual** (`select_free_block()`), para a Etapa 6 trocar só a escolha do bloco.
7. **Coeficiente de variação** adicionado à comparação, porque o desvio padrão sozinho não compara modos com médias diferentes.

**Figuras e evidências sugeridas**
- *Tela: `./simulador`*, mostrando o LBA 0 mudando de página a cada alteração, sem nenhum apagamento.
- *Tela: tabela comparativa do `./simulador --cenario-quente`*.
- *Gráfico de barras lado a lado: ciclos P/E por bloco* (`desgaste_direto.csv` e `desgaste_mapeamento.csv`).
  No direto, uma barra isolada em 1000. No mapeamento, os blocos 0 a 9 ficam entre 775 e 1000 ciclos, a mediana
  dos 128 blocos é 516 e o menos desgastado tem só 49: o desgaste melhorou, mas ainda é desigual.
- *Diagrama: fluxo de escrita com GC* (seção 5 da arquitetura).
- *Trecho de código: `collect_garbage()`*.

**Dificuldades e soluções**
- O GC precisa de espaço livre para copiar as páginas válidas da vítima, mas é acionado justamente quando falta espaço.
  *Solução:* acionar o GC com menos de 2 blocos livres, mantendo sempre 1 de reserva, e não deixar o próprio GC acionar outro GC.
- Num dispositivo usado antes pelo modo direto, podem existir cópias antigas de um LBA marcadas como válidas.
  *Solução:* o GC só copia a página para a qual a tabela aponta; as outras são descartadas no apagamento.
- Ao reabrir o dispositivo, o bloco que estava sendo preenchido ficava com páginas livres perdidas.
  *Solução:* a reconstrução identifica esse bloco e o retoma como bloco ativo.
- O cenário no modo com mapeamento faz milhões de operações (cerca de 30 segundos, quase tudo em gravação no arquivo).
  Foi viável graças à decisão da Etapa 3 de manter o arquivo aberto.

---

## Resumo para a seção de Desenvolvimento (segunda entrega)

Etapas executadas: 0 a 5 (ambiente, requisitos e modelagem, camada física, driver de E/S, FTL direta e FTL com mapeamento e GC).

| Camada | Arquivo | Verificações no `make test` |
|---|---|---|
| Física NAND | `NANDFlash.hpp` | 25 |
| Driver de E/S | `IODriver.hpp` | 27 |
| FTL direta | `FTL.hpp` (`DirectFTL`) | 20 |
| FTL com mapeamento e GC | `FTL.hpp` (`PageMappingFTL`) | 25 |
| **Total** | | **97** |

Próximos passos (entrega final): wear leveling dinâmico e estático (Etapa 6), sistema de arquivos mínimo (7),
shell interativo (8), métricas completas (9), bateria de testes comparativos com gráficos (10) e fechamento do artigo (11).

---

# Entrega final (Etapas 6 a 11)

## Etapa 6: Wear leveling dinâmico e estático (concluída)

**O que foi feito**
- Nova classe `WearLevelingFTL` (em `FTL.hpp`), derivada da `PageMappingFTL`. Reaproveita a tabela LBA → PBA,
  a escrita fora do lugar e o GC; muda só as políticas de desgaste.
- **Dinâmico:** `select_free_block()` passa a escolher, entre os blocos livres, o de **menor contador P/E**.
- **Estático:** a cada 64 escritas lógicas, compara o bloco livre mais gasto com o bloco de dados menos gasto.
  Se a diferença passar de **50 ciclos**, os dados frios são copiados para o bloco livre mais gasto e o bloco
  pouco gasto é apagado e volta à lista de livres, onde o dinâmico passa a usá-lo.
- O cenário `./simulador --cenario-quente` agora roda quatro modos: `direto`, `mapeamento`, `wl_dinamico` e `wl_estatico`.
- Novo teste `tests/test_wear_leveling.cpp` (17 verificações).

**Resultados do cenário de dados quentes (até a primeira falha de bloco)**

Valores **depois da correção feita na Etapa 7** (ver "Correção da Etapa 5" abaixo):

| Modo | Escritas na fase 2 | Média P/E | Desvio padrão | CV | Amplificação |
|---|---|---|---|---|---|
| Direto | 1.132 | 8,84 | 87,96 | 9,95 | 63,94 |
| Mapeamento sem WL | 625.777 | 236,04 | 142,72 | 0,60 | 3,09 |
| WL dinâmico | 2.601.162 | 984,34 | 87,38 | 0,09 | 3,10 |
| WL dinâmico + estático | 2.620.210 | 991,68 | 2,74 | 0,00 | 3,10 |

Interpretação para o texto:
- Neste cenário sintético, 10% das escritas caem em LBAs aleatórios: quase todo bloco acaba tendo páginas inválidas
  e passa pelo GC. Por isso o dinâmico sozinho já espalha bem o desgaste (vida 4,2 vezes maior que sem WL).
- O estático quase não muda a vida útil aqui (+0,7%), mas zera a diferença entre blocos: desvio de 87,38 para 2,74.
  No dinâmico ainda sobra 1 bloco nunca apagado (dados que nunca mudaram).
- O efeito do estático aparece de verdade na carga do sistema de arquivos (Etapa 7), onde há arquivos realmente frios.

**Decisões técnicas**
1. Wear leveling como **subclasse**: os modos comparados compartilham o mesmo código de mapeamento e GC, então a diferença
   medida vem só da política de desgaste.
2. Limite do estático de **50 ciclos** (5% da vida), como previsto na arquitetura; verificação a cada 64 escritas,
   para não percorrer os 128 blocos em toda escrita.
3. Os dados frios vão para o bloco livre **mais gasto**, que quase não será apagado de novo porque dados frios não mudam.

**Dificuldades e soluções**
- Na primeira versão, o estático só agia com mais de 2 blocos livres e **nunca migrou nada**: com o disco cheio,
  o GC mantém de 1 a 2 blocos livres. *Solução:* a migração não consome espaço (ocupa 1 bloco livre e devolve o bloco
  frio apagado), então basta haver 1 bloco livre.

**Figuras sugeridas**
- Gráfico de barras do desgaste por bloco nos quatro modos (`desgaste_<modo>.csv`).
- Trecho de código: `check_static_wear_leveling()` e `select_free_block()` da `WearLevelingFTL`.

## Etapa 7: Sistema de arquivos mínimo e gerador de carga (concluída)

**O que foi feito**
- `SimpleFS.hpp`: sistema de arquivos no estilo FAT, só com o necessário para gerar a carga de um pendrive.
  - LBA 0: superbloco (assinatura, espaço livre, número de arquivos, contador de operações).
  - LBAs 1 a 8: tabela de alocação (FAT), 2 bytes por LBA de dados.
  - LBA 9: diretório com até 32 arquivos (64 bytes por entrada).
  - LBAs 10 a 7679: área de dados (7.670 LBAs, cerca de 15 MB).
  - Operações: `format`, `mount`, `create`, `write_file`, `read_file`, `remove`, `list`.
  - **Toda operação regrava o superbloco, o diretório e as páginas da FAT que mudaram**: são os dados quentes.
- `Workload.hpp`: gerador de carga que imita o uso de um pendrive.
  - Preparação: arquivos grandes ("fotos" e "músicas", 256 a 900 KB) até ocupar 60% do disco. Nunca mudam (dados frios).
  - Uso: 70% editar um documento (2 a 16 KB), 20% criar um documento, 10% apagar. Semente fixa.
- Novo teste `tests/test_fs.cpp` (29 verificações).

**Correção da Etapa 5 encontrada nesta etapa**
- Ao medir a carga do sistema de arquivos, apareceram blocos parcialmente gravados que não eram o bloco ativo.
  Causa: quando o GC rodava, ele podia abrir um bloco ativo novo para copiar as páginas válidas, e logo depois a
  escrita abria **outro** bloco, abandonando as páginas livres do primeiro.
  *Solução:* a escrita só abre bloco novo se o bloco ativo ainda estiver cheio depois do GC.
- O GC também passou a contar as páginas livres que sobraram num bloco que não é o ativo como espaço recuperável
  (necessário depois das migrações do wear leveling estático, que podem deixar o fim do bloco livre).
- Com a correção, os números do mapeamento sem WL mudaram (1.345.345 → 625.777 escritas no cenário de dados quentes).
  **Os resultados da segunda entrega usavam a versão com o defeito**; os da entrega final usam a versão corrigida.
  Boa "dificuldade encontrada" para o artigo: o defeito só apareceu porque a nova carga foi medida bloco a bloco.

**Primeira medição com a carga do sistema de arquivos (até a primeira falha)**

| Modo | Escritas lógicas até a falha | Média P/E | Desvio padrão |
|---|---|---|---|
| Direto | 6.996 | 15,9 | 122,5 |
| Mapeamento sem WL | 879.105 | 106,3 | 265,5 |
| WL dinâmico | 1.027.841 | 124,5 | 296,5 |
| WL dinâmico + estático | 5.428.161 | 844,2 | 40,5 |

- Com arquivos realmente frios (60% do disco), o dinâmico quase não ajuda (+17%): os blocos das fotos nunca ficam livres.
- O estático move essas fotos para blocos gastos e libera os blocos novos: **vida útil 6,2 vezes maior** que sem WL.
- Esses números são revistos na Etapa 10, com as métricas completas.

**Decisões técnicas**
1. FAT com primeiro LBA livre (*first fit*), como o FAT real: os mesmos LBAs de dados são reutilizados.
2. Diretório de uma única página e FAT de 8 páginas, para concentrar os metadados em 10 LBAs.
3. O gerador de carga tem semente fixa e usa só a interface do SimpleFS, então roda igual nos quatro modos.

## Etapa 8: Shell interativo (concluída)

**O que foi feito**
- `Shell.hpp`: shell de comandos sobre o pendrive simulado. `./simulador` sem opções abre o shell.
- Comandos: `formatar <modo>`, `modo`, `ls`, `criar`, `escrever`, `ler`, `apagar`, `carga <n>`, `estatisticas`,
  `desgaste` (mapa 16 × 8 com os ciclos P/E de cada bloco), `ajuda` e `sair`.
- O modo da FTL é escolhido na formatação e salvo em `nand_config.txt`: cada modo organiza os dados de um jeito,
  então trocar de modo exige formatar.
- O desgaste é salvo depois de cada comando, então fechar o programa não perde nada.
- Aceita roteiros: `./simulador < comandos.txt`, útil para a apresentação.
- A demonstração antiga virou `./simulador --demo` e usa arquivos próprios.
- Novo teste `tests/test_shell.cpp` (14 verificações), que roda três sessões seguidas e confere a saída.

**Decisões técnicas**
1. Comandos em português e sem acentos na saída, para aparecer certo no CMD do Windows.
2. O shell só conhece a interface de cada camada (`SimpleFS`, `FTL`, `IODriver`), sem acessar os blocos por fora.
3. O `carga <n>` reaproveita o gerador da Etapa 7; se o disco estiver quase vazio, copia antes as fotos e músicas.

**Figura sugerida**
- Tela do shell: `formatar estatico`, `escrever`, `ler`, `carga 500`, `estatisticas` e `desgaste`.

## Etapa 9: Coleta de métricas (concluída)

**O que foi feito**
- `Metrics.hpp`: métricas calculadas a partir dos contadores P/E da camada física.
  - Resumo do desgaste: média, desvio padrão, coeficiente de variação (CV = desvio / média), mínimo, máximo,
    mediana, percentis 10 e 90, blocos nunca apagados, bad blocks e vida usada (média / 1000).
  - `WearTimeline`: evolução do desgaste ao longo das escritas. Começa com uma amostra a cada 50 escritas;
    quando passa de 400 amostras, descarta uma sim, outra não, e dobra o intervalo. Assim a curva tem sempre
    entre 200 e 400 pontos, tanto no modo direto (milhares de escritas) quanto no estático (milhões).
  - Exportação em CSV: desgaste por bloco (uma coluna por modo) e evolução.
- `Experiment.hpp`: um experimento formata um dispositivo próprio, aplica uma carga num modo da FTL até o
  primeiro bloco falhar e devolve todas as métricas. As duas cargas (dados quentes e sistema de arquivos)
  agora passam pelo mesmo código.
- `./simulador --cenario-quente` e `./simulador --cenario-fs` mostram a tabela comparativa e exportam
  `<cenario>_resumo.csv`, `<cenario>_desgaste_por_bloco.csv` e `<cenario>_evolucao_<modo>.csv`.
- No shell, `estatisticas` passou a mostrar o resumo completo do desgaste e o novo comando `exportar <arquivo.csv>`
  grava os ciclos P/E dos 128 blocos.
- Novo teste `tests/test_metrics.cpp` (22 verificações) e 2 verificações novas no teste do shell.
  `make test` passou a ter 179 verificações, todas passando.

**Por que essas métricas**
- O desvio padrão sozinho engana: um desvio de 87 é enorme se a média é 9, e pequeno se a média é 984.
  O **CV** compara modos com médias diferentes.
- A **vida usada** mostra quanto da capacidade total de apagamentos foi aproveitada até a primeira falha.
  Sem wear leveling, o pendrive "morre" com quase todos os blocos novos.
- A **amplificação de escrita (WA)** mede o custo: páginas gravadas na NAND por escrita lógica.

**Resultados com as métricas completas (escritas lógicas da fase de carga até a primeira falha)**

Cenário de dados quentes:

| Modo | Escritas | Média P/E | Desvio | CV | Mín | Vida usada | WA |
|---|---|---|---|---|---|---|---|
| Direto | 1.132 | 8,8 | 88,0 | 9,95 | 0 | 0,9% | 63,94 |
| Mapeamento sem WL | 625.777 | 236,0 | 142,7 | 0,60 | 0 | 23,6% | 3,09 |
| WL dinâmico | 2.601.162 | 984,3 | 87,4 | 0,09 | 0 | 98,4% | 3,10 |
| WL dinâmico + estático | 2.620.210 | 991,7 | 2,7 | 0,003 | 976 | 99,2% | 3,10 |

Cenário do sistema de arquivos:

| Modo | Escritas | Média P/E | Desvio | CV | Mín | Vida usada | WA |
|---|---|---|---|---|---|---|---|
| Direto | 2.013 | 15,9 | 122,5 | 7,71 | 0 | 1,6% | 59,18 |
| Mapeamento sem WL | 874.122 | 106,3 | 265,5 | 2,50 | 0 | 10,6% | 1,00 |
| WL dinâmico | 1.022.858 | 124,5 | 296,5 | 2,38 | 0 | 12,4% | 1,00 |
| WL dinâmico + estático | 5.423.178 | 844,2 | 40,5 | 0,05 | 813 | 84,4% | 1,01 |

- Os números do cenário quente batem com os da Etapa 7 (mesma semente), agora medidos pelo código comum.
- No cenário do sistema de arquivos, a WA fica perto de 1: as edições regravam o arquivo inteiro e o FS
  libera os LBAs antigos, então o GC encontra blocos quase todos inválidos.
- O estático custa pouco: 23.369 migrações em 5,4 milhões de escritas (WA de 1,00 para 1,01).

**Decisões técnicas**
1. Métricas calculadas só a partir dos contadores P/E da NAND, que a camada física já mantinha desde a Etapa 2.
2. Amostragem com intervalo que dobra, para a curva de evolução ter tamanho parecido em todos os modos.
3. Cada experimento usa arquivos próprios e os apaga no fim, para não estragar o pendrive do shell.

## Etapa 10: Testes comparativos, gráficos e tela de resultados (concluída)

**O que foi feito**
- `./simulador --comparar` roda as duas cargas (uso de pendrive e dados quentes) nos quatro modos da FTL,
  sempre com a mesma sequência de escritas, até a primeira falha de bloco. Mostra as tabelas no terminal,
  exporta os CSVs e grava a **tela de resultados** `resultados.html`, que abre sozinha no navegador.
  No shell, o comando `comparar` faz o mesmo. Leva cerca de 2 minutos.
- A tela de resultados atende à crítica da segunda entrega (o projeto era só terminal). Ela mostra:
  - destaque: quantas vezes mais escritas o wear leveling estático aguentou que o mapeamento sem WL;
  - escritas até a primeira falha (barras) e vida aproveitada dos blocos (medidores);
  - mapa de desgaste dos 128 blocos em cada modo (o bloco que falhou aparece com ✕);
  - histograma dos ciclos P/E (faixas de 100 ciclos);
  - evolução do bloco mais gasto e da média ao longo das escritas até a falha;
  - tabela com todas as métricas e um glossário ("Como ler esta página").
- `Report.hpp`: monta os resultados em JSON e grava a página; `ReportPage.hpp`: o modelo da página (HTML, CSS e
  JavaScript embutidos no executável). Passar o mouse mostra os valores de cada bloco e de cada ponto.
- `docs/resultados.md`: tabelas, figuras e análise; `docs/resultados.html`: exemplo da página já gerada.
- Novo teste `tests/test_report.cpp` (15 verificações). `make test`: 196 verificações, todas passando.

**Decisões técnicas**
1. Página HTML única, gerada pelo próprio programa em C++: roda em qualquer máquina com navegador, sem
   instalar biblioteca gráfica, sem internet e sem mudar a compilação (`g++ main.cpp` continua igual).
2. Os dados entram na página como JSON e os gráficos são desenhados em SVG pelo navegador, na largura da tela
   (funciona no computador e no celular, com tema claro ou escuro).
3. Uma cor por função: azul para o desgaste (mapa do mais claro ao mais escuro), laranja para o bloco mais
   gasto, vermelho com ✕ só para bad block. Os números estão sempre escritos, não só na cor.
4. O texto que vai para dentro do JSON é escapado (inclusive `<`), para nenhum nome quebrar a página.

**Resultado principal (para a seção de Resultados do artigo)**

| Carga | Sem WL | WL dinâmico | WL dinâmico + estático |
|---|---|---|---|
| Uso de pendrive: escritas até a falha | 874.122 | 1.022.858 (1,17×) | 5.423.178 (**6,2×**) |
| Uso de pendrive: vida aproveitada | 10,6% | 12,4% | **84,4%** |
| Dados quentes: escritas até a falha | 625.777 | 2.601.162 (4,2×) | 2.620.210 (**4,2×**) |
| Dados quentes: desvio padrão | 142,7 | 87,4 | **2,7** |

- O dinâmico basta quando todos os dados mudam de vez em quando; com dados frios de verdade (fotos e músicas),
  só o estático aproveita os blocos que guardam esses dados.
- O custo do estático é de cerca de 1% de escritas extras (WA de 1,00 para 1,01 no uso de pendrive).
- O mapeamento direto morre em cerca de 2 mil escritas: qualquer FTL com mapeamento já é centenas de vezes melhor.

**Figuras sugeridas** (em `docs/img/`)
- `resultados_fs_topo.png`: destaque, escritas até a falha e vida aproveitada.
- `resultados_mapas_fs.png`: mapa de desgaste dos 128 blocos nos quatro modos.
- `resultados_hist_fs.png` e `resultados_evolucao_fs.png`: distribuição e evolução do desgaste.
- `resultados_mapas_quente.png`: mapa de desgaste no cenário de dados quentes.
