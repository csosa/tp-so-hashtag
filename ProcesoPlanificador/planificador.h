#ifndef SRC_PLANIFICADOR_H_
#define SRC_PLANIFICADOR_H_

#include <commons/collections/list.h>
#include <stdlib.h>
#include "funcionesPlanificador.h"
#include <signal.h>
#include <semaphore.h>
#include <string.h>
#include <fcntl.h>
#include "socket_lib.h"
#include <unistd.h>

planificador * arch;
CPU_struct* argCPU;
pthread_t CPUThread;
int32_t socket_servidor;
sock_t * socket_general;

void inicializar_configuracion();
void manage_Planificador_shutdown();

#endif

