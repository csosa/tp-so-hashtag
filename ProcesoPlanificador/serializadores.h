#ifndef SERIALIZADORES_H_
#define SERIALIZADORES_H_

#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>
#include <commons/log.h>
#include <commons/config.h>
#include <stdlib.h>
#include <stdio.h>
#include <commons/collections/list.h>
#include <commons/collections/dictionary.h>
#include <time.h>
#include <semaphore.h>
#include <fcntl.h>
#include <pthread.h>

typedef struct pcb_t{
	int32_t pid;
	char * estado;
	char * path;
	int32_t cant_var_cont_actual;
	int32_t program_counter;
	int32_t retardo_io;
	time_t inicial;
}pcb_t;

typedef struct context_ejec{
	pcb_t * pcb;
	int quantum;
}context_ejec;

typedef struct cpu_t{
	int32_t fdCPU;
	int32_t id_proceso_exec;
	char * idCPU;
	int32_t porcentaje_uso;
} cpu_t;

pcb_t * crear_pcb(char  * ruta, char * estado, int32_t pid,int32_t pc, int32_t cantidad_proc);
context_ejec * crear_contexto_ejecucion(pcb_t * pcb, int quantum);
cpu_t * crear_estructura_cpu_nueva();
cpu_t * crear_estructura_cpu(int32_t fd, char * id);
void destruir_estructura_contexto(context_ejec * contexto);
void destruir_estructura_pcb(pcb_t * pcb);
void destruir_estructura_cpu(cpu_t * cpu);
char * serializar_contexto(context_ejec * contexto);
char * serializar_pcb(pcb_t * pcb);
char * serializar_cpu(cpu_t * cpu);
context_ejec * deserializar_contexto(char * contexto_serializado);
pcb_t * deserializar_pcb(char * pcb_serializado);
cpu_t * deserializar_cpu(char * cpu_serializada);
void sem_decre(sem_t *sem);
void sem_incre(sem_t *sem);
void destruir_mutex(pthread_mutex_t * mutex);
void crear_cont(sem_t *sem ,int32_t val_ini);
void * agregar_elemento_a_diccionario(t_dictionary * dict, char * key,
		void * value, pthread_mutex_t * mutex);
void * quitar_elemento_a_diccionario(t_dictionary * dict, char * key,
		pthread_mutex_t * mutex);
#endif /* SERIALIZADORES_H_ */
