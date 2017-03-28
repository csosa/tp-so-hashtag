#ifndef SRC_ADMINISTRADOR_DE_MEMORIA_H_
#define SRC_ADMINISTRADOR_DE_MEMORIA_H_

#include <commons/collections/list.h>
#include <commons/log.h>
#include <commons/config.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdbool.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>
#include "socket_lib.h"
#include "procesosMemoria.h"

t_log* logCPU;
char* archLogMemoria;
char* archLogSwap;
char * archLogCPU;
t_log* logSwap;

#endif /* SRC_ADMINISTRADOR_DE_MEMORIA_H_ */
