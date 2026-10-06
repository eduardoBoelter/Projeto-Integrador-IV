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

## Etapa 2: Camada física da NAND

*(a preencher ao concluir a etapa)*

## Etapa 3: Driver de E/S

*(a preencher)*

## Etapa 4: FTL com mapeamento direto

*(a preencher)*

## Etapa 5: FTL com mapeamento e garbage collection

*(a preencher)*
