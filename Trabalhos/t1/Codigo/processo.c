#include "processo.h"

int get_pid(processo_t *processo)
{
  return processo->pid;
}
int get_pc(processo_t *processo)
{
  return processo->pc;
}
int get_modo(processo_t *processo)
{
    return processo->modo;
}
int get_estado(processo_t *processo)
{
  return processo->estado;
}
int get_a(processo_t *processo)
{
  return processo->reg_a;
}
int get_x(processo_t *processo)
{
  return processo->reg_x;
}
int get_terminal(processo_t *processo)
{
  return processo->terminal;
}
int get_pid_esp(processo_t *processo)
{
  return processo->pid_esperado;
}
int get_motivo_bloq(processo_t *processo)
{
  return processo->motivo;
}
double get_prioridade(processo_t *processo)
{
  return processo->prioridade;
}


void set_pid(processo_t *processo, int valor)
{
  processo->pid = valor;
}
void set_pc(processo_t *processo, int valor)
{
  processo->pc = valor;
}
void set_modo(processo_t *processo, int valor)
{
  processo->modo = valor;
}
void set_estado(processo_t *processo, int valor)
{
  processo->estado = valor;
}
void set_a(processo_t *processo, int valor)
{
  processo->reg_a = valor;
}
void set_x(processo_t *processo, int valor)
{
  processo->reg_x = valor;
}
void set_terminal(processo_t *processo, int valor)
{
  processo->terminal = valor;
}
void set_pid_esp(processo_t *processo, int valor)
{
  processo->pid_esperado = valor;
}
void set_motivo_bloq(processo_t *processo, int valor)
{
  processo->motivo = valor;
}
void set_prioridade(processo_t *processo, double valor)
{
  processo->prioridade = valor;
}

