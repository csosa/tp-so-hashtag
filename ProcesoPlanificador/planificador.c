#include "planificador.h"


#define MAGENTA  "\x1B[35m"
#define BLANCO  "\x1B[37m"
void inicializar_configuracion(){

	printf(MAGENTA
			"              _^__                                      __^__\n"
			"            ( ___ )------------------------------------( ___ )   \n"
			"             | / |                                      | \\ |  \n"
			"             | / | Bienvenidos al Proceso Planificador! | \\ |   \n"
			"             |___|                                      |___| \n"
			"            (_____)------------------------------------(_____)\n");

	// Levanto el archivo de configuracion
	arch = levantarConfigPlanificador("planificador.config");
	//Creo logs para el planificador
	log_planificador = log_create("log_planificador.log", "#Hashtag", 0, LOG_LEVEL_INFO);
	log_pantalla = log_create("log_planificador.log", "#Hashtag", 1, LOG_LEVEL_INFO);
	log_info(log_planificador, "*************** INICIO DEL PROCESO PLANIFICADOR ***************");
}

void manage_Planificador_shutdown() {
	printf("Desea bajar la ejecucion del proceso Planificador? Si/No ");
	char respuesta[3];
	respuesta[2] = '\0';
	scanf("%s", respuesta);
	while (strcasecmp(respuesta, "Si") != 0 && strcasecmp(respuesta, "No") != 0) {
		printf("La opcion ingresada no es correcta. Por favor indique si desea dar de baja el planificador.\n");
		scanf("%s", respuesta);
	}
	if (strcasecmp(respuesta, "Si") == 0){
		printf(
				"    ╔═════════════════════════╗ \n"
				"    ║	  Saludos,  Hashtag   ║ \n"
				"    ╚═════════════════════════╝ \n");
		printf(BLANCO);
		exit(EXIT_SUCCESS);
	}
	 else
		printf("Mejor, no es recomendable bajar el planificador\n");

}

int main(){

	if( signal(SIGINT, manage_Planificador_shutdown) == SIG_ERR )
		log_info(log_planificador, "Se bajo al Planificador");

	inicializar_configuracion();
	//Seteo de estructura de parametros para los hilos
	CPU_struct* argCPU = crearParametrosCPU(arch, logCPU);
	individualCPU_struct* argCadaCPU=malloc(sizeof(individualCPU_struct));
	argCadaCPU->logger = logCPU;

	//Creo hilos de ejecucion
	int32_t hiloCreado = crearHiloCPUListener(&CPUThread, argCPU, log_planificador);
	if(hiloCreado!=0) exit(EXIT_FAILURE);

	//Ejecuto la consola
	pthread_t consola;
	pthread_create(&consola, NULL, (void *)lanzar_consola, (void *)arch);
	pthread_join(consola,NULL);
	log_destroy(log_planificador);
	log_destroy(log_pantalla);

	printf(
		            "    ╔═════════════════════════╗ \n"
					"    ║	  Saludos,  Hashtag   ║ \n"
					"    ╚═════════════════════════╝ \n");
	printf(BLANCO);
	return 0;
}
