#define N_ESTADOS 4

typedef enum motivo_bloqueio {
  OK = 1,
  LEITURA = 2,
  ESCRITA = 3,
  ESPERANDO_MORTE = 4
}motivo_bloqueio;
typedef enum estado_processo {
  PRONTO = 0,
  EXECUTANDO = 1,
  BLOQUEADO = 2,
  MORTO = 3
}estado_processo;

typedef enum modo_processo {
  KERNEL = 0,
  USUARIO = 1
}modo_processo;

typedef struct metricas_processo {
  int tempo_retorno;
  int preempcoes;
  int num_estado_pronto;
  int num_estado_executando;
  int num_estado_bloqueado;
  int tempo_estado_pronto;
  int tempo_estado_executando;
  int tempo_estado_bloqueado;
  int tempo_resposta;
} metricas_processo;

typedef struct processo_t {
  int pid;
  modo_processo modo;
  estado_processo estado;
  int pc;
  int reg_a;
  int reg_x;
  int terminal;
  int pid_esperado;
  motivo_bloqueio motivo;
  double prioridade;
  metricas_processo metricas;
} processo_t;

// getters
int get_pid(processo_t *processo);
int get_pc(processo_t *processo);
int get_modo(processo_t *processo);
int get_estado(processo_t *processo);
int get_a(processo_t *processo);
int get_x(processo_t *processo);
int get_terminal(processo_t *processo);
int get_pid_esp(processo_t *processo);
int get_motivo_bloq(processo_t *processo);
double get_prioridade(processo_t *processo);

// setters
void set_pid(processo_t *processo, int valor);
void set_pc(processo_t *processo, int valor);
void set_modo(processo_t *processo, int valor);
void set_estado(processo_t *processo, int valor);
void set_a(processo_t *processo, int valor);
void set_x(processo_t *processo, int valor);
void set_terminal(processo_t *processo, int valor);
void set_pid_esp(processo_t *processo, int valor);
void set_motivo_bloq(processo_t *processo, int valor);
void set_prioridade(processo_t *processo, double valor);
