#include <stdbool.h>
#include <commons/log.h>
#include <commons/config.h>
#include <commons/collections/list.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include "socket_lib.h"
#include "funcionesSwap.h"

sock_t * socketServer;

void manage_Swap_shutdown();
