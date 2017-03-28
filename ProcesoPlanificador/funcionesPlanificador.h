#ifndef FUNCIONESPLANIFICADOR_H_
#define FUNCIONESPLANIFICADOR_H_

#include <commons/log.h>
#include <commons/config.h>
#include <commons/collections/list.h>
#include <commons/collections/queue.h>
#include <commons/collections/dictionary.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>
#include "serializadores.h"
#include <semaphore.h>
#include <fcntl.h>
#include "socket_lib.h"
#include <sys/stat.h>
#include <pthread.h>

#define PROMPT "PLAN$>"
#define BLANCO  "\x1B[37m"
#define MAGENTA  "\x1B[35m"

//Defino estructuras
typedef struct CPU_struct{
	t_log* logger;
	int32_t puertoCPU;
} CPU_struct;

typedef struct{
	t_log* logger;
	sock_t* socketsIndividuales;
} individualCPU_struct;

int quantum;
t_config * configuracion;
t_log * logCPU;
pthread_t CPUThread;

typedef struct{
	int32_t puerto_escucha;
	int32_t quantum;
	char * algortimo_planificacion;
}planificador;

typedef struct{
	pcb_t * io;
	int32_t tiempo;
}thread_params;

typedef struct{
	int argc;
	char* mensaje[];
}thread_params2;

typedef struct{
	time_t inicial;
	int total;
}metrica;

//Comando
typedef int (*t_comando) (int argc, char *argv[]);

//Variables globales
int32_t quit_sistema;
int32_t pids;
int32_t cpus_c;
int32_t finalizar;

//Semaforos
sem_t cantidad_listos;
sem_t cantidad_exec;
sem_t cantidad_block;
sem_t cantidad_finish;
sem_t cantidad_cpu_libres;
sem_t cantidad_io;
sem_t finish;
sem_t io_ok;
sem_t archivo_ok;
sem_t listo_ok;
sem_t ejec_ok;

//Declaro colas y dic de cpu
t_list* cola_cpu_disponibles;
t_list* cola_cpu_conectadas;
t_dictionary * dic_cpus;
t_dictionary * tiempos_espera;
t_dictionary * tiempos_ejecucion;
t_dictionary * tiempos_respuesta;

//Loggers
t_log * log_planificador;
t_log * log_pantalla;

//Colas de procesos
t_queue * cola_procesos_listos;
t_queue * cola_procesos_ejecutandose;
t_queue * cola_procesos_suspendidos;
t_queue * cola_procesos_finalizados;
t_queue * cola_procesos_io;
t_queue * cola_cpu;
t_queue * cola_cpu_ocupadas;

//Declaro estructuras a utilizar
pcb_t * pcb;
planificador * archivo_planificador;
sock_t* socketServerCPU;

//Funciones
void manejador_ready_exec();
void * agregar_elemento_a_diccionario(t_dictionary * dict, char * key, void * value, pthread_mutex_t * mutex);
void * quitar_elemento_a_diccionario(t_dictionary * dict, char * key, pthread_mutex_t * mutex);
CPU_struct * crearParametrosCPU (planificador* , t_log*);
int32_t crearHiloCPUListener (pthread_t* , CPU_struct* , t_log*);
void * hiloCPUListener (void*);
void * hiloIndividualCPU (void*);
planificador * levantarConfigPlanificador(char * path);
void mandar_mProc_ejecutar();
int comando_correr(int argc, char* mensaje[]);
int comando_finalizar(int argc, char** mensaje);
t_dictionary * inicializar_diccionario_de_comandos();
void inicializar_diccionarios_y_colas();
void cerrar_todo(t_dictionary* dic_de_comandos);
int contar_strings_en_lista(char *string[]);
cpu_t *get_cpu_libre();
void mandar_porcentaje_cpu();
void ejecutar_comando(char * comando, t_dictionary *dic_de_comandos);
void obtener_comando(char ** buffer);
int lanzar_consola(void * config);
void agregar_pcb_cola_listos(pcb_t * pcb);
void run_to_finish(char * pcb_serializada, char * id_cpu, int32_t bandera, char * mensaje);
void dar_de_baja_cpu();
pcb_t *get_pcb_otros_exec(int32_t id_proc);
void actualizar_pcb(int32_t pid, int32_t program_counter_nuevo, int32_t tipo_cola);
void fin_quantum(cpu_t * aux_cpu, pcb_t * pcb_quantum, int32_t program_counter);
pcb_t * buscar_pcb(int32_t pid);
void nueva_cpu(cpu_t * cpu, int32_t nuevo_fd);
void manejador_IO();
void pasar_ejec_to_io(int32_t pid, int32_t retardo);
void guardar_porcentaje_cpu(cpu_t * cpu, int32_t porcentaje_nuevo);
int ayuda(int argc, char *argv[]);
void imprimir_cola(t_queue * cola, char * estado);
void pasar_io_to_ready(pcb_t * pcb_to_io);
cpu_t * buscar_cpu(int32_t pid);
void buscar_cpu_por_id(char * id);
cpu_t * buscar_cpu_por_pid(int32_t pid);
void manejador_fin_quantum();
void libero_cpu(int32_t pid, int32_t retardo_io, int bandera);
void pasar_ready_to_exec(pcb_t * pcb);
void proximo();
void buscar_pcb_y_actualizo_finalizar(int32_t pid);
cpu_t * buscar_cpu_sin_sacarla(char * id);
int actualizar_program_counter_para_finalizar(int32_t pid);
void actualizar_total_pcb(char * pid, int32_t total);
void finalizar2(char ** mensaje);
void mostrar_porcentaje();
pcb_t * buscar_pcb_sin_sacarla(int32_t pid);
void sumar_tiempo_ejecucion(pcb_t * pcb_new, pthread_mutex_t mutex, t_dictionary * diccionario, int32_t tipo);
void mostrar_mensaje_rafaga(char * mensaje_serializado, int32_t pid);
void sumar_tiempo_espera(pcb_t * pcb_new);
void imprimir_cola_en_log(t_queue * cola, char * estado);

#endif /* FUNCIONESPLANIFICADOR_H_ */
