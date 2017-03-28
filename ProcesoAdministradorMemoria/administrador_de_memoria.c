#include "administrador_de_memoria.h"
#include "procesosMemoria.h"

#define ROJO     "\x1b[31m"
#define CYAN     "\x1b[36m"
#define BLANCO   "\x1B[37m"
void iniciar_configuracion(){


	printf(CYAN
		            "              _^__                                      __^__\n"
					"            ( ___ )------------------------------------( ___ )   \n"
					"             | / |                                      | \\ |  \n"
					"             | / |    Bienvenidos al proceso Memoria    | \\ |   \n"
					"             |___|                                      |___| \n"
					"            (_____)------------------------------------(_____)\n");


	/* Levanto el archivo de configuración*/
		crear_estructura_config("memoria.config");

	//	Creo logs para los hilos
		archLogMemoria = "administrador_de_memoria.c";
		logMemoriaPantalla = log_create("logFileMemoria.log", archLogMemoria, 1, LOG_LEVEL_INFO);
		logMemoriaOculto = log_create("logFileMemoria.log", archLogMemoria, 0, LOG_LEVEL_INFO);

		archLogSwap= "hiloSwap.c";
		logSwap = log_create("logFileSwap.txt", archLogSwap,1,LOG_LEVEL_INFO);

		archLogCPU= "hiloCPU.c";
		logCPU = log_create("logFileCpu.txt", archLogCPU,1,LOG_LEVEL_INFO);
}

void manage_Memoria_shutdown() {
	printf("Desea bajar la ejecucion del proceso Memoria? Si/No ");
	char respuesta[3];
	respuesta[2] = '\0';
	scanf("%s", respuesta);
	while (strcasecmp(respuesta, "Si") != 0 && strcasecmp(respuesta, "No") != 0) {
		printf(
				"La opcion ingresada no es correcta. Por favor indique si desea dar de baja la memoria.\n");
		scanf("%s", respuesta);
	}
	if(strcasecmp(respuesta, "Si") == 0){
		enviar_string(socketClienteSwap->fd,"6");
		cerrarMemoria();
		printf(BLANCO);
		exit(EXIT_SUCCESS);
	}
	else{
		printf("Mejor, no es recomendable bajar la memoria\n");
	}
}

int main() {

	if( signal(SIGINT, manage_Memoria_shutdown) == SIG_ERR ) {
		//  Acá hay que agregar el logeo al logger de la memoria
	}
	if( signal(SIGUSR1, crearHiloLimpiarTLB) == SIG_DFL ) {
		//  Acá hay que agregar el logeo al logger de la memoria
	}
	if( signal(SIGUSR2, crearHiloLimpiarMemoria) == SIG_DFL ) {
		//  Acá hay que agregar el logeo al logger de la memoria
	}
	if( signal(SIGIO, crearHiloVolcarMemoria) == SIG_DFL ) {
		//  Acá hay que agregar el logeo al logger de la memoria
	}

	iniciar_configuracion();
	inicializar_diccionariosYSemaforos();
	recibi_mensaje = false;

	//Seteo de estructura de parametros para los hilos
	Swap_struct* argSwap = crearParametrosSwap(configuracion,logSwap);
	CPU_struct* argCPU = crearParametrosCPU(configuracion, logCPU);
	individualCPU_struct* argCadaCPU=malloc(sizeof(individualCPU_struct));
	argCadaCPU->logger = logCPU;


	//creo socket para el swap
	SwapFunction(argSwap);


	if(seCreoElClienteSwap){

		// Creo hilo cpu listener
		pthread_t CPUThread;
		crearHiloCPUListener(&CPUThread, argCPU, logMemoriaOculto);

		if(string_equals_ignore_case(configuracion->tlb_habilitada, "si") ){
			pthread_t estadisticasTLBThread;
			crearHiloEstadisticasTLB(&estadisticasTLBThread);
		}


		//Espero a que termine la ejecucion el hilo de cpu listener para finalizar el proceso
		pthread_join(CPUThread,NULL);

	}
	else{
		cerrarMemoria();
	}
	printf(
				"                       ╔═══════════════════════════════╗ \n"
				"                       ║   Me guSSSSta la memoria <3   ║ \n"
				"                       ╚═══════════════════════════════╝\n");


	exit(EXIT_SUCCESS);

}
