// so.c
// sistema operacional
// simulador de computador
// so24b

// INCLUDES {{{1
#include "so.h"
#include "dispositivos.h"
#include "irq.h"
#include "programa.h"
#include "instrucao.h"
#include "memoria.h"
#include "processo.h"

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

// CONSTANTES E TIPOS {{{1
// intervalo entre interrupções do relógio
//configurações do SO
#define INTERVALO_INTERRUPCAO 50   // em instruções executadas
#define MAX_PROCESSOS 10
#define QUANTUM 10
#define ESCALONADOR 3

#define ESCALONADOR_BASE 1
#define ESCALONADOR_CIRCULAR 2
#define ESCALONADOR_PRIORITARIO 3

const char *nomes_escalonadores[] = {
  "escalonador_inválido",
  "escalonador_base",
  "escalonador_circular",
  "escalonador_prioritário"
};

typedef struct metricas_so {
  int total_processos;
  int total_preempcoes;
  int total_interrupcoes[N_IRQ];
  int tempo_total;
  int tempo_total_ocioso;
} metricas_so_t;

typedef struct so_t {
  cpu_t *cpu;
  mem_t *mem;
  es_t *es;
  console_t *console;
  bool erro_interno;
  processo_t **tabela_processos;
  processo_t **fila_prontos;
  processo_t *processo_corrente;
  int contador_pid;
  int quantum;
  int relogio;
  metricas_so_t metricas_so;
} so_t;

// função de tratamento de interrupção (entrada no SO)
static int so_trata_interrupcao(void *argC, int reg_a);

// funções auxiliares
// carrega o programa contido no arquivo na memória do processador; retorna end. inicial
static int so_carrega_programa(so_t *self, char *nome_do_executavel);
// copia para str da memória do processador, até copiar um 0 (retorna true) ou tam bytes
static bool copia_str_da_mem(int tam, char str[tam], mem_t *mem, int ender);

void adiciona_fila(so_t *self, processo_t *processo);
void remove_fila(so_t *self);
int so_calcula_terminal(int terminal, int tipo);
processo_t *so_busca_processo(so_t *self, int pid);
// CRIAÇÃO {{{1


//funções que tratam métricas
int get_total_preempcoes(so_t *self)
{
  int total_preempcoes = 0;
  for(int i = 0; i < self->contador_pid - 1; i++)
  {
    processo_t *processo = self->tabela_processos[i];
    total_preempcoes += processo->metricas.preempcoes;
  }
  return total_preempcoes;
}

static void inicializa_metricas_processo(processo_t *processo)
{
  processo->metricas.tempo_retorno = 0;
  processo->metricas.preempcoes = 0;
  processo->metricas.tempo_resposta = 0;

  processo->metricas.num_estado_pronto = 1;
  processo->metricas.tempo_estado_pronto = 0;
  processo->metricas.num_estado_executando = 0;
  processo->metricas.tempo_estado_executando = 0;
  processo->metricas.num_estado_bloqueado = 0;
  processo->metricas.tempo_estado_bloqueado = 0;
}

static void inicializa_metricas_so(so_t *self)
{
  self->metricas_so.total_processos = 0;
  self->metricas_so.total_preempcoes = 0;
  self->metricas_so.tempo_total = 0;
  self->metricas_so.tempo_total_ocioso = 0;

  for(int i = 0; i < N_IRQ; i++)
  {
  self->metricas_so.total_interrupcoes[i] = 0;
  }
}

static void atualiza_metricas_processo(processo_t *processo, int tempo_decorrido)
{
  if(processo == NULL) return;
  if (processo->estado != MORTO){
    processo->metricas.tempo_retorno += tempo_decorrido;
  }
  int estado = get_estado(processo);
  switch (estado)
  {
  case PRONTO:
    processo->metricas.tempo_estado_pronto += tempo_decorrido;
    break;

  case EXECUTANDO:
    processo->metricas.tempo_estado_executando += tempo_decorrido;
    break;

    case BLOQUEADO:
    processo->metricas.tempo_estado_bloqueado += tempo_decorrido;
    break;

  default:
    break;
  }
  processo->metricas.tempo_resposta = processo->metricas.tempo_estado_pronto / processo->metricas.num_estado_pronto;
}

static void atualiza_metricas_so(so_t *self, int tempo_decorrido)
{
  self->metricas_so.tempo_total += tempo_decorrido;
  if (self->processo_corrente == NULL)
  {
    self->metricas_so.tempo_total_ocioso += tempo_decorrido;
  }
  for (int i = 0; i < self->contador_pid - 1; i++)
  {
    atualiza_metricas_processo(self->tabela_processos[i], tempo_decorrido);
  }
}

static void calcula_metricas(so_t *self)
{
  int relogio_ant = self->relogio;
  if (es_le(self->es, D_RELOGIO_INSTRUCOES, &self->relogio) != ERR_OK)
  {
    console_printf("SO: problema no acesso ao relógio");
    return;
  }

  if (relogio_ant != -1)
  {
    int tempo_decorrido = self->relogio - relogio_ant;
    atualiza_metricas_so(self, tempo_decorrido);
  }
}

static void printa_metricas(so_t *self)
{
    // Atualiza as métricas do SO
    self->metricas_so.total_preempcoes = get_total_preempcoes(self);

    // Define o nome do arquivo
    char nome[100];
    sprintf(nome, "../Relatório e métricas/métricas_so_%s.txt", nomes_escalonadores[ESCALONADOR]);

    // Tenta abrir o arquivo
    FILE *arq = fopen(nome, "w");
    if (arq == NULL) {
        console_printf("SO: problema na abertura do arquivo de métricas: %s\n", nome);
        return;
    }

    // Imprime métricas gerais
    fprintf(arq, "=====================================================\n");
    fprintf(arq, "               MÉTRICAS DO SISTEMA OPERACIONAL       \n");
    fprintf(arq, "=====================================================\n");
    fprintf(arq, " Tempo total em execução     : %d\n", self->metricas_so.tempo_total);
    fprintf(arq, " Tempo total ocioso          : %d\n", self->metricas_so.tempo_total_ocioso);
    fprintf(arq, " Número total de processos   : %d\n", self->contador_pid - 1);
    fprintf(arq, " Número total de preempções  : %d\n", self->metricas_so.total_preempcoes);
    fprintf(arq, "\n");

    for (int i = 0; i < N_IRQ; i++) {
        fprintf(arq, " Interrupções tipo %d         : %d\n", i, self->metricas_so.total_interrupcoes[i]);
    }

    fprintf(arq, "\n=====================================================\n");
    fprintf(arq, "               MÉTRICAS DOS PROCESSOS                \n");
    fprintf(arq, "=====================================================\n");

    // Imprime métricas por processo
    for (int i = 0; i < self->contador_pid - 1; i++) {
        if (self->tabela_processos == NULL || self->tabela_processos[i] == NULL) {
            console_printf("SO: processo na tabela está inválido\n");
            continue;
        }

        processo_t *processo = self->tabela_processos[i];
        fprintf(arq, "-----------------------------------------------------\n");
        fprintf(arq, " Processo PID                  : %d\n", processo->pid);
        fprintf(arq, " Tempo de retorno              : %d\n", processo->metricas.tempo_retorno);
        fprintf(arq, " Tempo médio de resposta       : %d\n", processo->metricas.tempo_resposta);
        fprintf(arq, " Número de preempções          : %d\n", processo->metricas.preempcoes);
        fprintf(arq, "\n");

        fprintf(arq, " Estado PRONTO:\n");
        fprintf(arq, "   - Vezes no estado           : %d\n", processo->metricas.num_estado_pronto);
        fprintf(arq, "   - Tempo no estado           : %d\n", processo->metricas.tempo_estado_pronto);
        fprintf(arq, "\n");

        fprintf(arq, " Estado EXECUTANDO:\n");
        fprintf(arq, "   - Vezes no estado           : %d\n", processo->metricas.num_estado_executando);
        fprintf(arq, "   - Tempo no estado           : %d\n", processo->metricas.tempo_estado_executando);
        fprintf(arq, "\n");

        fprintf(arq, " Estado BLOQUEADO:\n");
        fprintf(arq, "   - Vezes no estado           : %d\n", processo->metricas.num_estado_bloqueado);
        fprintf(arq, "   - Tempo no estado           : %d\n", processo->metricas.tempo_estado_bloqueado);
        fprintf(arq, "-----------------------------------------------------\n");
    }

    fprintf(arq, "=====================================================\n");

    // Fecha o arquivo
    fclose(arq);
}


//funções que tratam pendências
void verifica_leitura(so_t *self, processo_t *processo)
{
  int estado;
  int terminal = get_terminal(processo);

  if(es_le(self->es, so_calcula_terminal(terminal, TECLADO_OK), &estado) == ERR_OK && estado != 0) 
  {
    set_estado(processo, PRONTO);
    set_motivo_bloq(processo, OK);
    set_a(processo, 0);
    adiciona_fila(self, processo);
  }
  return;
}

void verifica_escrita(so_t *self, processo_t *processo)
{
  int estado;
  int terminal = get_terminal(processo);


  if(es_le(self->es, so_calcula_terminal(terminal, TELA_OK), &estado) != ERR_OK) {
      console_printf("SO: problema no acesso ao estado da tela");
      self->erro_interno = true; 
      return;
  }
  if(estado == 0) return;

  int dado = get_x(processo);

  if(es_escreve(self->es, so_calcula_terminal(terminal, TELA), dado) == ERR_OK)
  {
    set_estado(processo, PRONTO);
    set_motivo_bloq(processo, OK);
    set_a(processo, 0);
    adiciona_fila(self, processo);
  } 
  return;
}

void verifica_morte(so_t *self, processo_t *processo)
{
  processo_t *processo_esperado = so_busca_processo(self, get_pid_esp(processo));
  if(processo_esperado->estado == MORTO) 
  {
    set_estado(processo, PRONTO);
    set_motivo_bloq(processo, OK);
    set_a(processo, 0);
    adiciona_fila(self, processo);
    return;
  }
}


//funções que tratam escalonadores
void adiciona_fila(so_t *self, processo_t *processo)
{
  int i = 0;
  while (self->fila_prontos[i] != NULL){
    i++;
  }
  self->fila_prontos[i] = processo;
  return;
    
console_printf("FILA DE PROCESSOS PRONTOS CHEIA");
} 

void remove_fila(so_t *self)
{
  for (int i = 0; i < MAX_PROCESSOS - 1; i++) {
    self->fila_prontos[i] = self->fila_prontos[i + 1];
  }
  self->fila_prontos[MAX_PROCESSOS - 1] = NULL;
}

double calcula_prioridade(so_t *self, processo_t *processo)
{
  return (processo->prioridade + (QUANTUM - self->quantum) / (float)QUANTUM) / 2;
}

int tam_fila(so_t *self)
{
  int i = 0;
  while (self->fila_prontos[i] != NULL){
    i++;
  }
  return i;
}

void ordena_fila(so_t *self){
  processo_t *aux;
  for (int i = 0; i < tam_fila(self); i++){
    for (int j = i + 1; j < tam_fila(self); j++){
      if (self->fila_prontos[i]->prioridade > self->fila_prontos[j]->prioridade){
        aux = self->fila_prontos[i];
        self->fila_prontos[i] = self->fila_prontos[j];
        self->fila_prontos[j] = aux;
      }
    }
  }
}

static void so_escalona_prioritario(so_t *self)
{
  // escolhe o próximo processo a executar, que passa a ser o processo
  //   corrente; pode continuar sendo o mesmo de antes ou não
  // t1: na primeira versão, escolhe um processo caso o processo corrente não possa continuar
  //   executando. depois, implementar escalonador melhor
  if(self->processo_corrente != NULL && self->processo_corrente->estado == EXECUTANDO && self->quantum > 0) return;

  if(self->processo_corrente != NULL && self->processo_corrente->estado == EXECUTANDO && self->quantum == 0)
  {
    set_estado(self->processo_corrente, PRONTO);
    set_motivo_bloq(self->processo_corrente, OK);
    set_prioridade(self->processo_corrente, calcula_prioridade(self, self->processo_corrente));
    adiciona_fila(self, self->processo_corrente);
    self->processo_corrente->metricas.preempcoes++;
  }

  if(self->fila_prontos[0] != NULL)
  {
    ordena_fila(self);
    self->processo_corrente = self->fila_prontos[0];
    set_estado(self->processo_corrente, EXECUTANDO);
    self->quantum = QUANTUM;
    remove_fila(self);
    return;
  }
  self->processo_corrente = NULL; 
}

static void so_escalona_circular(so_t *self)
{
  // escolhe o próximo processo a executar, que passa a ser o processo
  //   corrente; pode continuar sendo o mesmo de antes ou não
  // t1: na primeira versão, escolhe um processo caso o processo corrente não possa continuar
  //   executando. depois, implementar escalonador melhor
  if(self->processo_corrente != NULL && self->processo_corrente->estado == EXECUTANDO && self->quantum > 0) return;

  if(self->processo_corrente != NULL && self->processo_corrente->estado == EXECUTANDO && self->quantum == 0)
  {
    set_estado(self->processo_corrente, PRONTO);
    set_motivo_bloq(self->processo_corrente, OK);
    self->processo_corrente->metricas.preempcoes++;
    adiciona_fila(self, self->processo_corrente);
  }

  if(self->fila_prontos[0] != NULL)
  {
    self->processo_corrente = self->fila_prontos[0];
    set_estado(self->processo_corrente, EXECUTANDO);
    self->quantum = QUANTUM;
    remove_fila(self);
    return;
  }
  self->processo_corrente = NULL; 
}

static void so_escalona_base(so_t *self)
{
  // escolhe o próximo processo a executar, que passa a ser o processo
  //   corrente; pode continuar sendo o mesmo de antes ou não
  // t1: na primeira versão, escolhe um processo caso o processo corrente não possa continuar
  //   executando. depois, implementar escalonador melhor
  
  if(self->processo_corrente != NULL && self->processo_corrente->estado == EXECUTANDO) return;

  for(int i = 0; i < self->contador_pid - 1; i++)
  {
    processo_t *processo = self->tabela_processos[i];
    if(processo->estado == PRONTO)
    {
      set_estado(processo, EXECUTANDO);
      self->processo_corrente = processo;
      return;
    }
  }
  self->processo_corrente = NULL;
}


//funções que tratam processos
processo_t *so_inicializa_processo(int pid, int pc)
{
  processo_t *novo_processo = malloc(sizeof(processo_t));
  set_pid(novo_processo, pid);
  set_pc(novo_processo, pc);
  set_a(novo_processo, 0);
  set_x(novo_processo, 0);
  set_estado(novo_processo, PRONTO);
  set_motivo_bloq(novo_processo, OK);
  set_modo(novo_processo, USUARIO);
  set_terminal(novo_processo, ((pid - 1) % 4) * 4);
  set_prioridade(novo_processo, 0.5);
  inicializa_metricas_processo(novo_processo);

  return novo_processo;
}

processo_t *so_cria_processo(so_t *self, char *nome_programa)
{
  int endereco = so_carrega_programa(self, nome_programa);
  if (endereco == -1) return NULL;

  processo_t *novo_processo = so_inicializa_processo(self->contador_pid, endereco);
  if(novo_processo == NULL) return NULL;

  for(int i = 0; i < MAX_PROCESSOS - 1; i++)
  {
    if(self->tabela_processos[i] == NULL)
    {
      self->tabela_processos[i] = novo_processo;
      self->contador_pid++;
      break;
    }
  }
  adiciona_fila(self, novo_processo);
  return novo_processo;
}

processo_t *so_busca_processo(so_t *self, int pid)
{
  for(int i = 0; i < self->contador_pid - 1; i++)
  {
    if(self->tabela_processos[i]->pid == pid)
    {
      return self->tabela_processos[i];
    }
  }
  console_printf("PROCESSO NAO ENCONTRADO DURANTE BUSCA");
  return NULL;
}

// funções do so

int so_calcula_terminal(int terminal, int tipo)
{
  return terminal + tipo;
}

so_t *so_cria(cpu_t *cpu, mem_t *mem, es_t *es, console_t *console)
{
  so_t *self = malloc(sizeof(*self));
  if (self == NULL) return NULL;

  self->cpu = cpu;
  self->mem = mem;
  self->es = es;
  self->console = console;
  self->erro_interno = false;
  self->tabela_processos = malloc(MAX_PROCESSOS * sizeof(processo_t));
  self->fila_prontos = malloc (MAX_PROCESSOS * sizeof(processo_t));
  self->processo_corrente = NULL;
  self->contador_pid = 1;
  self->quantum = QUANTUM;
  self->relogio = -1;
  inicializa_metricas_so(self);
  // quando a CPU executar uma instrução CHAMAC, deve chamar a função
  //   so_trata_interrupcao, com primeiro argumento um ptr para o SO
  cpu_define_chamaC(self->cpu, so_trata_interrupcao, self);


  // coloca o tratador de interrupção na memória
  // quando a CPU aceita uma interrupção, passa para modo supervisor, 
  //   salva seu estado à partir do endereço 0, e desvia para o endereço
  //   IRQ_END_TRATADOR
  // colocamos no endereço IRQ_END_TRATADOR o programa de tratamento
  //   de interrupção (escrito em asm). esse programa deve conter a 
  //   instrução CHAMAC, que vai chamar so_trata_interrupcao (como
  //   foi definido acima)
  int ender = so_carrega_programa(self, "trata_int.maq");
  if (ender != IRQ_END_TRATADOR) 
  {
    console_printf("SO: problema na carga do programa de tratamento de interrupção");
    self->erro_interno = true;
  }

  // programa o relógio para gerar uma interrupção após INTERVALO_INTERRUPCAO
  if (es_escreve(self->es, D_RELOGIO_TIMER, INTERVALO_INTERRUPCAO) != ERR_OK) 
  {
    console_printf("SO: problema na programação do timer");
    self->erro_interno = true;
  }

  return self;
}

void so_destroi(so_t *self)
{
  cpu_define_chamaC(self->cpu, NULL, NULL);
  free(self);
}

static int termina_so(so_t *self)
{
  err_t e1, e2;
  e1 = es_escreve(self->es, D_RELOGIO_TIMER, 0);
  e2 = es_escreve(self->es, D_RELOGIO_INTERRUPCAO, 0);
  if (e1 != ERR_OK || e2 != ERR_OK)
  {
    console_printf("SO: nao consigo desligar o timer!!");
    self->erro_interno = true;
  }
  printa_metricas(self);
  console_printf("Metricas salvas com sucesso.");
  console_printf("====================================================");
  console_printf("           Todos os processos estao mortos.       ");
  console_printf("         Pressione 'F' para encerrar o SO.    ");
  console_printf("====================================================");

  return 1;
}

// TRATAMENTO DE INTERRUPÇÃO {{{1

// funções auxiliares para o tratamento de interrupção
static void so_salva_estado_da_cpu(so_t *self);
static void so_trata_irq(so_t *self, int irq);
static void so_trata_pendencias(so_t *self);
static void so_escalona(so_t *self);
static int so_despacha(so_t *self);

// função a ser chamada pela CPU quando executa a instrução CHAMAC, no tratador de
//   interrupção em assembly
// essa é a única forma de entrada no SO depois da inicialização
// na inicialização do SO, a CPU foi programada para chamar esta função para executar
//   a instrução CHAMAC
// a instrução CHAMAC só deve ser executada pelo tratador de interrupção
//
// o primeiro argumento é um ponteiro para o SO, o segundo é a identificação
//   da interrupção
// o valor retornado por esta função é colocado no registrador A, e pode ser
//   testado pelo código que está após o CHAMAC. No tratador de interrupção em
//   assembly esse valor é usado para decidir se a CPU deve retornar da interrupção
//   (e executar o código de usuário) ou executar PARA e ficar suspensa até receber
//   outra interrupção


static int so_trata_interrupcao(void *argC, int reg_a)
{
  so_t *self = argC;
  irq_t irq = reg_a;
  // esse print polui bastante, recomendo tirar quando estiver com mais confiança
  //console_printf("SO: recebi IRQ %d (%s)", irq, irq_nome(irq));
  calcula_metricas(self);
  // salva o estado da cpu no descritor do processo que foi interrompido
  so_salva_estado_da_cpu(self);
  // faz o atendimento da interrupção
  so_trata_irq(self, irq);
  // faz o processamento independente da interrupção
  so_trata_pendencias(self);
  // escolhe o próximo processo a executar
  so_escalona(self);
  // recupera o estado do processo escolhido
  for(int i = 0; i < self->contador_pid - 1; i++)
  {
    if(self->tabela_processos[i]->estado != MORTO) return so_despacha(self);
  }
  return termina_so(self);
}

static void so_salva_estado_da_cpu(so_t *self)
{
  // t1: salva os registradores que compõem o estado da cpu no descritor do
  //   processo corrente. os valores dos registradores foram colocados pela
  //   CPU na memória, nos endereços IRQ_END_*
  // se não houver processo corrente, não faz nada
  if(self->processo_corrente == NULL) return;

  int pc, a, x;

  mem_le(self->mem, IRQ_END_PC, &pc);
  mem_le(self->mem, IRQ_END_A, &a);
  mem_le(self->mem, IRQ_END_X, &x);

  set_pc(self->processo_corrente, pc);
  set_a(self->processo_corrente, a);
  set_x(self->processo_corrente, x);
}

static void so_trata_pendencias(so_t *self)
{
  // t1: realiza ações que não são diretamente ligadas com a interrupção que
  //   está sendo atendida:
  // - E/S pendente
  // - desbloqueio de processos
  // - contabilidades
  if(!self->tabela_processos) return;

  for(int i = 0; i < self->contador_pid - 1; i++)
  {
    processo_t *processo_bloqueado = self->tabela_processos[i];

    if(processo_bloqueado != NULL && get_estado(processo_bloqueado) == BLOQUEADO)
    {
      int motivo = get_motivo_bloq(processo_bloqueado);

      switch (motivo)
      {
      case LEITURA:
        verifica_leitura(self, processo_bloqueado);
        break;
        
      case ESCRITA:
        verifica_escrita(self, processo_bloqueado);
        break;

      case ESPERANDO_MORTE:
        verifica_morte(self, processo_bloqueado);
        break;

      default:
        break;
      }
    }
  }
}

static void so_escalona(so_t *self)
{
  switch (ESCALONADOR)
  {
  case ESCALONADOR_BASE:
    so_escalona_base(self);
    break;

  case ESCALONADOR_CIRCULAR:
    so_escalona_circular(self);
    break;

  case ESCALONADOR_PRIORITARIO:
    so_escalona_prioritario(self);
    break;
  
  default:
    break;
  }
}

static int so_despacha(so_t *self)
{
  // t1: se houver processo corrente, coloca o estado desse processo onde ele
  //   será recuperado pela CPU (em IRQ_END_*) e retorna 0, senão retorna 1
  // o valor retornado será o valor de retorno de CHAMAC
  if(self->processo_corrente == NULL || self->erro_interno) return 1;

  mem_escreve(self->mem, IRQ_END_PC, get_pc(self->processo_corrente));
  mem_escreve(self->mem, IRQ_END_A, get_a(self->processo_corrente));
  mem_escreve(self->mem, IRQ_END_X, get_x(self->processo_corrente));
  return 0;
}

// TRATAMENTO DE UMA IRQ {{{1

// funções auxiliares para tratar cada tipo de interrupção
static void so_trata_irq_reset(so_t *self);
static void so_trata_irq_chamada_sistema(so_t *self);
static void so_trata_irq_err_cpu(so_t *self);
static void so_trata_irq_relogio(so_t *self);
static void so_trata_irq_desconhecida(so_t *self, int irq);

static void so_trata_irq(so_t *self, int irq)
{
  self->metricas_so.total_interrupcoes[irq]++;
  // verifica o tipo de interrupção que está acontecendo, e atende de acordo
  switch (irq) {
    case IRQ_RESET:
      so_trata_irq_reset(self);
      break;
    case IRQ_SISTEMA:
      so_trata_irq_chamada_sistema(self);
      break;
    case IRQ_ERR_CPU:
      so_trata_irq_err_cpu(self);
      break;
    case IRQ_RELOGIO:
      so_trata_irq_relogio(self);
      break;
    default:
      so_trata_irq_desconhecida(self, irq);
  }
}

// interrupção gerada uma única vez, quando a CPU inicializa
static void so_trata_irq_reset(so_t *self)
{
  // coloca o programa init na memória
  int ender = so_carrega_programa(self, "init.maq");

  if (ender != 100) {
    console_printf("SO: problema na carga do programa inicial");
    self->erro_interno = true;
    return;
  }
  processo_t *novo_processo = so_inicializa_processo(self->contador_pid, ender);
  self->processo_corrente = novo_processo;
  self->processo_corrente->estado = EXECUTANDO;
  self->tabela_processos[self->contador_pid - 1] = novo_processo;
  console_printf("FIZ O PROCESSO INIT");
  self->contador_pid++;
}

// interrupção gerada quando a CPU identifica um erro
static void so_trata_irq_err_cpu(so_t *self)
{
  // Ocorreu um erro interno na CPU
  // O erro está codificado em IRQ_END_erro
  // Em geral, causa a morte do processo que causou o erro
  // Ainda não temos processos, causa a parada da CPU
  int err_int;
  // t1: com suporte a processos, deveria pegar o valor do registrador erro
  //   no descritor do processo corrente, e reagir de acordo com esse erro
  //   (em geral, matando o processo)
  mem_le(self->mem, IRQ_END_erro, &err_int);
  err_t err = err_int;
  console_printf("SO: IRQ não tratada -- erro na CPU: %s", err_nome(err));
  self->erro_interno = true;
}

// interrupção gerada quando o timer expira
static void so_trata_irq_relogio(so_t *self)
{
  // rearma o interruptor do relógio e reinicializa o timer para a próxima interrupção
  err_t e1, e2;
  e1 = es_escreve(self->es, D_RELOGIO_INTERRUPCAO, 0); // desliga o sinalizador de interrupção
  e2 = es_escreve(self->es, D_RELOGIO_TIMER, INTERVALO_INTERRUPCAO);
  if (e1 != ERR_OK || e2 != ERR_OK) {
    console_printf("SO: problema da reinicialização do timer");
    self->erro_interno = true;
  }
  // t1: deveria tratar a interrupção
  //   por exemplo, decrementa o quantum do processo corrente, quando se tem
  //   um escalonador com quantum
  if(self->quantum > 0) self->quantum--;}

// foi gerada uma interrupção para a qual o SO não está preparado
static void so_trata_irq_desconhecida(so_t *self, int irq)
{
  console_printf("SO: não sei tratar IRQ %d (%s)", irq, irq_nome(irq));
  self->erro_interno = true;
}

// CHAMADAS DE SISTEMA {{{1

// funções auxiliares para cada chamada de sistema
static void so_chamada_le(so_t *self);
static void so_chamada_escr(so_t *self);
static void so_chamada_cria_proc(so_t *self);
static void so_chamada_mata_proc(so_t *self);
static void so_chamada_espera_proc(so_t *self);

static void so_trata_irq_chamada_sistema(so_t *self)
{
  // a identificação da chamada está no registrador A
  // t1: com processos, o reg A tá no descritor do processo corrente
  int id_chamada;
  if (mem_le(self->mem, IRQ_END_A, &id_chamada) != ERR_OK) {
    console_printf("SO: erro no acesso ao id da chamada de sistema");
    self->erro_interno = true;
    return;
  }
  //console_printf("SO: chamada de sistema %d", id_chamada);
  switch (id_chamada) {
    case SO_LE:
      so_chamada_le(self);
      break;
    case SO_ESCR:
      so_chamada_escr(self);
      break;
    case SO_CRIA_PROC:
      so_chamada_cria_proc(self);
      break;
    case SO_MATA_PROC:
      so_chamada_mata_proc(self);
      break;
    case SO_ESPERA_PROC:
      so_chamada_espera_proc(self);
      break;
    default:
      console_printf("SO: chamada de sistema desconhecida (%d)", id_chamada);
      // t1: deveria matar o processo
      self->erro_interno = true;
  }
}


static void bloqueia_processo(so_t *self, processo_t *processo, int motivo_bloq)
{
  set_estado(processo, BLOQUEADO);
  set_motivo_bloq(processo, motivo_bloq);
  set_prioridade(processo, calcula_prioridade(self, processo));
}

// implementação da chamada se sistema SO_LE
// faz a leitura de um dado da entrada corrente do processo, coloca o dado no reg A
static void so_chamada_le(so_t *self)
{
   //   T1: deveria realizar a leitura somente se a entrada estiver disponível,
  //     senão, deveria bloquear o processo.
  //   no caso de bloqueio do processo, a leitura (e desbloqueio) deverá
  //     ser feita mais tarde, em tratamentos pendentes em outra interrupção,
  //     ou diretamente em uma interrupção específica do dispositivo, se for
  //     o caso
  processo_t *corrente = self->processo_corrente;
  int terminal = get_terminal(corrente);
 
  int estado;

  if(es_le(self->es, so_calcula_terminal(terminal, TECLADO_OK), &estado) != ERR_OK) 
  {
    console_printf("SO: problema no acesso ao estado do teclado");
    self->erro_interno = true;
    return;
  }

  if (estado == 0) 
  {
    bloqueia_processo(self, corrente, LEITURA);
    return;
  }

  int dado = get_x(corrente);

  if (es_escreve(self->es, so_calcula_terminal(terminal, TECLADO), dado) != ERR_OK) {
    console_printf("SO: problema no acesso à tela");
    self->erro_interno = true;
    return;
  }
  set_a(corrente, dado);
}

// implementação da chamada se sistema SO_ESCR
// escreve o valor do reg X na saída corrente do processo
static void so_chamada_escr(so_t *self)
{
  processo_t *corrente = self->processo_corrente;
  int terminal = get_terminal(corrente);
 
  int estado;

  if (es_le(self->es, so_calcula_terminal(terminal, TELA_OK), &estado) != ERR_OK) {
    console_printf("SO: problema no acesso ao estado da tela");
    self->erro_interno = true;
    return;
  }

  if (estado == 0) 
  {
    bloqueia_processo(self, corrente, ESCRITA);
    return;
  }

  int dado = get_x(corrente);

  if (es_escreve(self->es, so_calcula_terminal(terminal, TELA), dado) != ERR_OK) {
    console_printf("SO: problema no acesso à tela");
    self->erro_interno = true;
    return;
  }
  set_a(corrente, 0);
}

// implementação da chamada se sistema SO_CRIA_PROC
// cria um processo
static void so_chamada_cria_proc(so_t *self)
{
  // em X está o endereço onde está o nome do arquivo

  int ender_proc = get_pc(self->processo_corrente);
  // t1: deveria ler o X do descritor do processo criador
  if (mem_le(self->mem, IRQ_END_X, &ender_proc) == ERR_OK) {
    char nome[100];
    if (copia_str_da_mem(100, nome, self->mem, ender_proc)) {
      processo_t *novo_processo = so_cria_processo(self, nome);
      int ender_carga = novo_processo->pc;
      if (ender_carga > 0) {
        // t1: deveria escrever no PC do descritor do processo criado
        set_a(self->processo_corrente, get_pid(novo_processo));
        return;
      } // else?
    }
  }
  // deveria escrever -1 (se erro) ou o PID do processo criado (se OK) no reg A
  //   do processo que pediu a criação
  //mem_escreve(self->mem, IRQ_END_A, -1);
}

void mata_processo(so_t *self, int pid)
{
  console_printf("MATANDO PROCESSO DE PID %d", pid);
  if(pid > MAX_PROCESSOS || pid <= 0) return;

  processo_t *processo = so_busca_processo(self, pid);
  if (processo != NULL) 
  {
    set_estado(processo, MORTO);
  }
  set_a(processo, 0);
}

// implementação da chamada se sistema SO_MATA_PROC
// mata o processo com pid X (ou o processo corrente se X é 0)
static void so_chamada_mata_proc(so_t *self)
{
  processo_t *corrente = self->processo_corrente;
  int pid_matarao = get_x(corrente);

  if(pid_matarao == 0) mata_processo(self, get_pid(corrente));
  
  else if(corrente != NULL) mata_processo(self, pid_matarao);

  else set_a(corrente, -1);
}

// implementação da chamada se sistema SO_ESPERA_PROC
// espera o fim do processo com pid X
static void so_chamada_espera_proc(so_t *self)
{
  // T1: deveria bloquear o processo se for o caso (e desbloquear na morte do esperado)
  // ainda sem suporte a processos, retorna erro -1
  processo_t *corrente = self->processo_corrente;
  int pid_esperado = get_x(corrente);
  processo_t *processo_esperado = so_busca_processo(self, pid_esperado);

  if(processo_esperado == NULL || processo_esperado == corrente)
  {
    set_a(corrente, -1);
    return;
  }

  bloqueia_processo(self, corrente, ESPERANDO_MORTE);
  set_pid_esp(corrente, pid_esperado);
}


// CARGA DE PROGRAMA {{{1

// carrega o programa na memória
// retorna o endereço de carga ou -1
static int so_carrega_programa(so_t *self, char *nome_do_executavel)
{
  // programa para executar na nossa CPU
  programa_t *prog = prog_cria(nome_do_executavel);
  if (prog == NULL) {
    console_printf("Erro na leitura do programa '%s'\n", nome_do_executavel);
    return -1;
  }

  int end_ini = prog_end_carga(prog);
  int end_fim = end_ini + prog_tamanho(prog);

  for (int end = end_ini; end < end_fim; end++) {
    if (mem_escreve(self->mem, end, prog_dado(prog, end)) != ERR_OK) {
      console_printf("Erro na carga da memória, endereco %d\n", end);
      return -1;
    }
  }

  prog_destroi(prog);
  console_printf("SO: carga de '%s' em %d-%d", nome_do_executavel, end_ini, end_fim);
  return end_ini;
}


// ACESSO À MEMÓRIA DOS PROCESSOS {{{1

// copia uma string da memória do simulador para o vetor str.
// retorna false se erro (string maior que vetor, valor não char na memória,
//   erro de acesso à memória)
// T1: deveria verificar se a memória pertence ao processo
static bool copia_str_da_mem(int tam, char str[tam], mem_t *mem, int ender)
{
  for (int indice_str = 0; indice_str < tam; indice_str++) {
    int caractere;
    if (mem_le(mem, ender + indice_str, &caractere) != ERR_OK) {
      return false;
    }
    if (caractere < 0 || caractere > 255) {
      return false;
    }
    str[indice_str] = caractere;
    if (caractere == 0) {
      return true;
    }
  }
  // estourou o tamanho de str
  return false;
}



// vim: foldmethod=marker