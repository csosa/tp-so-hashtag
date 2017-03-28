#ifndef SRC_CPU_H_
#define SRC_CPU_H_

#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <signal.h>
#include <stdio.h>
#include "socket_lib.h"
#include "serializadores.h"
#include <stdlib.h>
#include <commons/log.h>
#include <commons/config.h>
#include <commons/collections/dictionary.h>
#include <pthread.h>
#include <time.h>

// Estructuras

typedef struct CPU{
	char* ip_planificador;
	int32_t puerto_planificador;
	char* ip_memoria;
	int32_t puerto_memoria;
	int32_t cantidad_hilos;
	int32_t retardo;
}CPU;

typedef struct hiloCPU{
	int32_t id;
	float porcentaje;
}hiloCPU;

typedef struct mensaje{
	int32_t socket;
	context_ejec * contexto;
	cpu_t * cpu;
}mensaje;

typedef struct porcentaje_t{
	double porcentaje;
	time_t inicio_fin;
	int comienzo;
}porcentaje_t;

// Variables Globales
CPU * cpu;
t_log * log_cpu;
t_log * log_pantalla;
t_config * archConfig;
context_ejec * contexto;
bool recibi_mensaje;
t_dictionary * tiempos_parciales;
t_dictionary * porcentajes_cpus;
t_dictionary * cpu_muertos;
int32_t quit_sistema;
cpu_t* cpu_nueva;
t_dictionary * hilos_cpu;
char * archLogCPU;
int finalizar;
sem_t listos;
sem_t exec;
sem_t block;
sem_t finish;
sem_t io;

// Funciones
void inciar_configuracion();
void crearLog();
void setearStructCPU();
void crearHilosCPU();
void pedirMemoriaParaVariablesGlobales();
void conectarConPlanificador();
void recibirMensajesDePlanificador(int32_t socket_planificador, cpu_t * cpu);
void conectarConMemoria();
void pedirMemoriaParaVariablesGlobales();
void manage_CPU_shutdown();
int contar_strings_en_lista(char *string[]);
int32_t recorrer_archivo_mProd(mensaje * mensaje);
int enviar_iniciar_memoria(int32_t cant_paginas, context_ejec * contexto, char * id);
char * enviar_leer_memoria(int32_t nro_paginas, context_ejec * contexto);
char * enviar_escribir_memoria(int32_t nro_paginas, char * texto, context_ejec * contexto);
void enviar_entrada_salida(int32_t tiempo, context_ejec * contexto, char * id, int32_t socket, int32_t pc, char * mensaje);
void enviar_finalizar_memoria(context_ejec * contexto, char * id, char * mensaje);
void enviar_fin_quantum(context_ejec * contexto, int32_t pc, char * id, int32_t socket, char * mensaje);
char * armado_mensajes_planificador(char * codigo, char * mensaje, char * anterior);

#endif /* SRC_CPU_H_ */
