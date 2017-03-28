#ifndef PROCESOSMEMORIA_H_
#define PROCESOSMEMORIA_H_

#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <signal.h>
#include <commons/log.h>
#include <commons/config.h>
#include <commons/collections/list.h>
#include <commons/collections/queue.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include "socket_lib.h"

typedef struct memoria{
	int32_t puerto_escucha;
	char * ip_swap;
	int32_t puerto_swap;
	int32_t maximo_marcos_por_proceso;
	int32_t cantidad_marcos;
	int32_t tamanio_marcos;
	int32_t entradas_tlb;
	char * tlb_habilitada;
	int32_t retardo_memoria;
	char* algoritmoDeReemplazo;
} memoria;

typedef struct Swap_struct{
	t_log* logger;
	int32_t puerto;
	char* ip;
} Swap_struct;

typedef struct CPU_struct{
	t_log* logger;
	int32_t puertoCPU;
	int32_t puertoSwap;
	char* ip_Swap;
} CPU_struct;

typedef struct
{
	t_log* logger;
	sock_t* socketsIndividuales;
	int32_t puertoSwap;
	char* ip_Swap;
} individualCPU_struct;

typedef struct{
	char* pagina;
	int32_t marco;
	int32_t seModifico;
	int32_t bitUso;
} PaginaFrame;

typedef struct{
	char* proceso;
	char* pagina;
	int32_t marco;
	int32_t seModifico;
} EntradaTLB;

typedef struct{
	char* codigoDelProceso;
	char* pagina;
	char* pid;
	char* stringAEscribir;
} MsjRecibido;

typedef struct{
	bool encontro;
	EntradaTLB * entradaBuscada;
} RespuestaBusquedaTLB;

typedef struct{
	bool encontro;
	PaginaFrame * entradaBuscada;
} RespuestaBusquedaTablaPaginas;

typedef struct{
	int32_t cantidadPaginasAccedidas;
	int32_t cantidadFallosDePagina;
	int32_t accesosASwap;
} EstadisticasProceso;

typedef struct{
	int bitOcupado;
	char* contenidoMarco;
} Marco;


bool recibi_mensaje;
int32_t handshake(sock_t* socketToSend, int32_t codeRequest);
Swap_struct* crearParametrosSwap(memoria* , t_log* );
CPU_struct* crearParametrosCPU(memoria* , t_log* );
int32_t crearHiloSwap(pthread_t* , Swap_struct* , t_log* );
void crearHiloCPUListener(pthread_t* , CPU_struct* , t_log* );
void SwapFunction(void* );
void *hiloCPUListener(void* );
void hiloIndividualCPU(void* );
void crearHiloLimpiarTLB();
void crearHiloLimpiarMemoria();
void crearHiloVolcarMemoria();
char * mensaje;
t_log* logMemoriaOculto;
t_log* logMemoriaPantalla;
bool seCreoElClienteSwap;
sock_t* socketClienteSwap;
sock_t* socketServerCPU;
char* respuestaLocal[1024];
int consultasTotales;
int consultasAcertadasPorTLB;
bool finalizarMemoria;

//estructuras de memoria
t_dictionary * tablaProcesos;
t_dictionary * estadisticasProcesos;
void crear_estructura_config(char* path);
t_queue * TLB;
t_list * tablaDeMarcos;
memoria* configuracion;


//semaforos muteX
pthread_mutex_t * mutex_tablaDePaginas;
pthread_mutex_t * mutex_TLB;
pthread_mutex_t * mutex_tablaDeMarcos;

#endif /* PROCESOSMEMORIA_H_ */
