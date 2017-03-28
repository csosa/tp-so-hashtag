#include "cpu.h"
#include <time.h>

#define AMARILLO "\x1B[33m"
#define BLANCO  "\x1B[37m"
pthread_mutex_t	mutex_cpu = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t	mutex_hilos = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t	mutex_procesos = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t	mutex_porcentaje = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t	mutex_archivo = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t	mutex_inactivo = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t	mutex_tiempos = PTHREAD_MUTEX_INITIALIZER;

int main() {

	inciar_configuracion();

	if (signal(SIGINT, manage_CPU_shutdown) == SIG_ERR){
		log_info(log_cpu,"Se bajo a la cpu");		   }

	printf(AMARILLO
			"             __^__                                      __^__\n"
			"            ( ___ )------------------------------------( ___ )   \n"
			"             | / |                                      | \\ |  \n"
			"             | / |      Bienvenidos al proceso CPU      | \\ |   \n"
			"             |___|                                      |___| \n"
			"            (_____)------------------------------------(_____)\n");

	conectarConPlanificador();
	return 0;
}

void inciar_configuracion(){
	hilos_cpu = dictionary_create();
	porcentajes_cpus = dictionary_create();
	tiempos_parciales = dictionary_create();
	cpu_muertos = dictionary_create();
	quit_sistema = 1;

	//Inicializo los semaforos
	crear_cont(&listos, 1);
	crear_cont(&block, 0);
	crear_cont(&exec, 1);
	crear_cont(&finish, 0);
	crear_cont(&io, 0);

	crearLog();
	setearStructCPU();
	crearHilosCPU();
}

void crearLog(){
	archLogCPU = strdup("cpu.c");
	log_cpu = log_create("log_cpu.log", archLogCPU, 0, LOG_LEVEL_INFO);
	log_pantalla = log_create("log_cpu.log", archLogCPU, 1, LOG_LEVEL_INFO);
	log_info(log_cpu, "****************** INICIO DEL PROCESO CPU ******************");
}

void setearStructCPU(){

	archConfig = config_create("CPU.config");
	cpu = malloc(sizeof(CPU));
	cpu->ip_planificador = config_get_string_value(archConfig, "IP_PLANIFICADOR");
	cpu->puerto_planificador = config_get_int_value(archConfig, "PUERTO_PLANIFICADOR");
	cpu->ip_memoria= config_get_string_value(archConfig, "IP_MEMORIA");
	cpu->puerto_memoria = config_get_int_value(archConfig, "PUERTO_MEMORIA");
	cpu->cantidad_hilos = config_get_int_value(archConfig, "CANTIDAD_HILOS");
	cpu->retardo = config_get_int_value(archConfig, "RETARDO");
}

void calculo_porcentaje(int32_t id){
	int sumatoria = 0;
	double porcentaje = 0;
	porcentaje_t * estructura_porcentaje = malloc(sizeof(porcentaje_t));
	while(quit_sistema){
		char * idstring = string_itoa(id);

		pthread_mutex_lock(&mutex_tiempos);
		if(dictionary_has_key(tiempos_parciales, idstring)){
			dictionary_remove(tiempos_parciales, idstring);
			dictionary_put(tiempos_parciales, idstring, (void*)0);
			pthread_mutex_unlock(&mutex_tiempos);
			sleep(60);
		}
		else{
			pthread_mutex_unlock(&mutex_tiempos);
			continue;
		}

		pthread_mutex_lock(&mutex_tiempos);
		if(dictionary_has_key(tiempos_parciales, idstring)){
			pthread_mutex_lock(&mutex_porcentaje);
			if(dictionary_has_key(porcentajes_cpus, idstring))
				estructura_porcentaje = dictionary_remove(porcentajes_cpus, idstring);
			sumatoria = (int)dictionary_get(tiempos_parciales, idstring);
			porcentaje = ((double)sumatoria/60)*100;
			if(porcentaje > 100)
				estructura_porcentaje->porcentaje = 100;
			else
				estructura_porcentaje->porcentaje = porcentaje;
			dictionary_put(porcentajes_cpus, idstring, (void*)estructura_porcentaje);
			pthread_mutex_unlock(&mutex_porcentaje);
			pthread_mutex_unlock(&mutex_tiempos);
			log_info(log_cpu, "[CPU ID: %s] Porcentaje actualizado %: %f.",idstring, estructura_porcentaje->porcentaje);
		}
	}
}

void cerrar_todo(){

	quit_sistema = 0;
	dictionary_destroy(hilos_cpu);
	dictionary_destroy(porcentajes_cpus);
	dictionary_destroy(tiempos_parciales);
	dictionary_destroy(cpu_muertos);
	pthread_mutex_destroy(&mutex_cpu);
	pthread_mutex_destroy(&mutex_hilos);
	pthread_mutex_destroy(&mutex_inactivo);
	pthread_mutex_destroy(&mutex_porcentaje);
	pthread_mutex_destroy(&mutex_procesos);
	pthread_mutex_destroy(&mutex_archivo);

	printf(
					"    ╔═════════════════════════╗ \n"
					"    ║	  Saludos,  Hashtag   ║ \n"
					"    ╚═════════════════════════╝\n");
	printf(BLANCO);
}

char * mensaje_porcentaje(char * key){
	porcentaje_t * por_plan = dictionary_get(porcentajes_cpus, key);
	int sumatoria = (int)por_plan->porcentaje;
	char * mensaje = string_new();
	string_append(&mensaje, "P|");
	string_append(&mensaje, key);
	string_append(&mensaje, "|");
	string_append(&mensaje, string_itoa(sumatoria));
	return mensaje;
}

void crearHilosCPU(){
	int i;
	for(i = cpu->cantidad_hilos; i >= 1 ; i--){
		pthread_t hilo;
		pthread_mutex_lock(&mutex_tiempos);
		dictionary_put(tiempos_parciales,string_itoa(i), (void*)0);
		pthread_mutex_unlock(&mutex_tiempos);
		pthread_create(&hilo, NULL, (void *)conectarConPlanificador, (void *)i);
	}
}

char*  armar_mensaje( char** cadena){
	u_int32_t n = 2;
	char* respuesta = string_new();
	while(cadena[n]!='\0' ){
		string_append(&respuesta,cadena[n]);
		string_append(&respuesta," ");
		n++;
	}
	 string_trim(&respuesta);
	 return respuesta;
}

char * armado_mensajes_planificador(char * codigo, char * mensaje, char * anterior){
	string_append(&anterior, codigo);
	string_append(&anterior, "|");
	string_append(&anterior, mensaje);
	string_append(&anterior, "%");
	return anterior;
};

void conectarConPlanificador(void * id_cpu){

	int32_t id = (int32_t)id_cpu;
	if(id > cpu->cantidad_hilos)
		pthread_exit(NULL);

	//Me conecto al planificador
	pthread_mutex_lock(&mutex_cpu);
	int32_t socket_planificador = crear_socket_cliente_conectado(cpu->ip_planificador, string_itoa(cpu->puerto_planificador));

	if(socket_planificador == -1){
		log_error(log_cpu, "Error al conectar con el proceso planificador");
		exit(EXIT_FAILURE);
	}

	//Me conecto con la memoria
	int32_t socket_memoria = crear_socket_cliente_conectado(cpu->ip_memoria, string_itoa(cpu->puerto_memoria));

	if(socket_memoria == -1){
		log_error(log_cpu, "Error al conectar con el proceso Memoria");
		printf(BLANCO);
		exit(EXIT_FAILURE);
	}
	else
		enviar_string(socket_memoria, "7|");

	cpu_t * cpu_new = crear_estructura_cpu_nueva(id);


	//Creo hilos de calculo de procentaje
	pthread_t porcentaje;
	pthread_create(&porcentaje, NULL, (void *)calculo_porcentaje, (void *)id);

	log_info(log_pantalla, "[CPU ID: %s] Conexion de CPU con socket: %d",cpu_new->idCPU, socket_planificador);
	//Le mando N de nueva mas el id de la cpu
	char * mje = string_new();
	string_append(&mje, "N|");
	string_append(&mje, serializar_cpu(cpu_new));

	enviar_string(socket_planificador, mje);
	free(mje);
	mje = NULL;

	pthread_mutex_unlock(&mutex_cpu);

	recibirMensajesDePlanificador(socket_planificador, cpu_new);
}

void recibirMensajesDePlanificador(int32_t socket_planificador, cpu_t * cpu_new){
	while(quit_sistema){
		context_ejec * c;
		//Espero que el planificador me responda

		char * mensajeLOCAL = recibir_y_deserializar_mensaje(socket_planificador);

		char codigo = mensajeLOCAL[0];
		char ** aux;
		char * mandar;
		int32_t socket_plan;
		switch(codigo){
		case 'P':
			pthread_mutex_lock(&mutex_hilos);
			socket_plan = crear_socket_cliente_conectado(cpu->ip_planificador,
					string_itoa(cpu->puerto_planificador));
			mandar = mensaje_porcentaje(cpu_new->idCPU);
			enviar_string(socket_plan, mandar);
			socket_plan = 0;
			free(mandar);
			pthread_mutex_unlock(&mutex_hilos);
			break;
		case 'N':
			aux = string_split(mensajeLOCAL,"&");
			c = deserializar_contexto(aux[1]);
			mensaje * mensaje = malloc(sizeof(mensaje));
			mensaje->contexto = c;
			mensaje->cpu = cpu_new;
			mensaje->socket = socket_planificador;
			log_info(log_cpu, "\n[CPU ID: %s] Contexto de Ejecucion recibido: \n"
								"PID: %d \n"
								"Nombre archivo: %s\n"
								"Program Counter: %d\n"
								"Quantum: %d\n", cpu_new->idCPU, c->pcb->pid, c->pcb->path,c->pcb->program_counter, c->quantum);
			int32_t b = recorrer_archivo_mProd(mensaje);
			if(b == 1){}
				//puts("Sali por fin de archivo o E/S o por quantum");
			else{
				mensaje->contexto->pcb->program_counter = b;
				recorrer_archivo_mProd(mensaje);
			}
			free(aux);
			break;
		default: break;
		}

		free(mensajeLOCAL);
		mensajeLOCAL = NULL;
	}
}

void manage_CPU_shutdown() {
	printf("Desea bajar la ejecucion del proceso CPU? Si/No ");
	char respuesta[3];
	respuesta[2] = '\0';
	scanf("%s", respuesta);
	while (strcasecmp(respuesta, "Si") != 0 && strcasecmp(respuesta, "No") != 0) {
		printf("La opcion ingresada no es correcta. Por favor indique si desea dar de baja la CPU.\n");
		scanf("%s", respuesta);
	}
	if(strcasecmp(respuesta, "Si") == 0) {
		int32_t socket_planificador2 = crear_socket_cliente_conectado(cpu->ip_planificador,string_itoa(cpu->puerto_planificador));
		int32_t socket_memoria= crear_socket_cliente_conectado(cpu->ip_memoria,string_itoa(cpu->puerto_memoria));
		char * mje2 = string_new();
		string_append(&mje2, "X|0");
		enviar_string(socket_planificador2, mje2);
		enviar_string(socket_memoria, "6");
		log_info(log_pantalla, "Se bajaron los hilos CPU, abortando.. ");
		log_info(log_cpu, "************FIN PROCESO CPU************ \n\n\n");
		free(mje2);
		cerrar_todo();
		exit(EXIT_SUCCESS);
	}
}

int contar_strings_en_lista(char *string[]){
    int i;
    for(i=0; string[i] != NULL; i++);
    return i;
}

void sumar_tiempo_ejecucion(time_t inicial, cpu_t * cpu_new){
	pthread_mutex_lock(&mutex_tiempos);
	double diferenciaTotal = difftime(time(NULL), inicial);
	int intermedio = 0;
	if(dictionary_has_key(tiempos_parciales, cpu_new->idCPU)){
		intermedio = (int)dictionary_get(tiempos_parciales, cpu_new->idCPU);
		dictionary_remove(tiempos_parciales, cpu_new->idCPU);
	}
	int total = (int)(diferenciaTotal + intermedio);
	dictionary_put(tiempos_parciales, cpu_new->idCPU, (void*)total);
	pthread_mutex_unlock(&mutex_tiempos);
}

int32_t recorrer_archivo_mProd(mensaje* mensaje){

	context_ejec * contexto = mensaje->contexto;
	cpu_t * cpu_new = mensaje->cpu;
	int32_t socket = mensaje->socket;

	int32_t socket_plan = crear_socket_cliente_conectado(cpu->ip_planificador,
			string_itoa(cpu->puerto_planificador));

	//Abro archivo mProd
	FILE* archivo = fopen(contexto->pcb->path,"rb");
	char cadena[100];
	int i = 0;

	char * instrucciones = string_new();
	char * archivo_ok = string_new();

	if(archivo == NULL){
		log_error(log_pantalla, "Error al abrir el archivo");
		exit(EXIT_FAILURE);
	}
	else{
		archivo_ok = string_new();
		string_append(&archivo_ok, "A&");
		string_append(&archivo_ok, cpu_new->idCPU);
		enviar_string(socket_plan, archivo_ok);
		log_info(log_cpu, "Archivo abierto correctamente");
	}

	free(archivo_ok);

	//Se recorre el archivo y voy apendeando el contenido de cada linea
	while(fgets(cadena, 100, archivo)!= NULL){
		cadena[string_length(cadena)-1] = '\0';
		string_append(&instrucciones, cadena);
		++i;
	}
	fclose(archivo);
	int j, k;
	char ** rafagas = string_split(instrucciones,";");
	rafagas[i] = '\0';

	for(k = 0; rafagas[k] !=NULL ;k++);
	int cantidad_total = k;
	int actual = 0;
	int limite = 0;
	free(instrucciones);

	if(contexto->quantum == 0)
		limite = 1000;
	else
		limite = contexto->quantum;

	contexto->pcb->cant_var_cont_actual = cantidad_total;
	sem_wait(&listos);
	pthread_mutex_lock(&mutex_archivo);
	char * mensaje_planificador = string_new();
	for(j = contexto->pcb->program_counter; j < cantidad_total ; j++){
		char ** parametros = string_split(rafagas[j]," ");

		//SE lee cada renglon del archivo y me fijo si se pasa del quantum etc
		if((actual <= 100) && (!string_equals_ignore_case(parametros[0],"entrada-salida"))){

			if(string_equals_ignore_case(parametros[0],"iniciar")){
				pthread_mutex_lock(&mutex_procesos);
				int ok = enviar_iniciar_memoria(atoi(parametros[1]),contexto, cpu_new->idCPU);
				pthread_mutex_lock(&mutex_porcentaje);
				//dictionary_put(tiempos_parciales, cpu_new->idCPU, (void*)0);
				porcentaje_t * p = malloc(sizeof(porcentaje_t));
				p->porcentaje = 0;
				dictionary_put(porcentajes_cpus, cpu_new->idCPU, (void*)p);
				pthread_mutex_unlock(&mutex_porcentaje);
				sem_post(&listos);
				mensaje_planificador = armado_mensajes_planificador("I", parametros[1], mensaje_planificador);
				if(ok == 0){
					log_error(log_pantalla, "[CPU ID: %s] mProc PID: %d ERROR al iniciar proceso, abortando..", cpu_new->idCPU,
							contexto->pcb->pid);
					pthread_mutex_unlock(&mutex_procesos);
					sem_post(&listos);
					pthread_mutex_unlock(&mutex_archivo);
					break;
				}
				else;
					pthread_mutex_unlock(&mutex_procesos);
			}

			if(string_equals_ignore_case(parametros[0],"leer")){
				actual++;
				if(actual <= limite){
					pthread_mutex_lock(&mutex_procesos);
					time_t inicial = time(NULL);
					char * leido = string_new();
					usleep(cpu->retardo*1000);
					char * contenido = enviar_leer_memoria(atoi(parametros[1]),contexto);
					sumar_tiempo_ejecucion(inicial, cpu_new);
					char ** contenido_separado = string_split(contenido, "|");
					if(string_equals_ignore_case(contenido_separado[0], "1")){
						log_info(log_pantalla, "[CPU ID: %s] mProc PID: %d Leyo '%s' de la pagina: %d", cpu_new->idCPU,
								contexto->pcb->pid, contenido_separado[1], atoi(parametros[1]));
						string_append(&leido, parametros[1]);
						string_append(&leido, "|");
						string_append(&leido, contenido_separado[1]);
						mensaje_planificador = armado_mensajes_planificador("L", leido, mensaje_planificador);
					}
					pthread_mutex_unlock(&mutex_procesos);
					free(contenido_separado);
				}
				else{
					actual = 0;
					pthread_mutex_lock(&mutex_procesos);
					enviar_fin_quantum(contexto, j, cpu_new->idCPU, socket, mensaje_planificador);
					pthread_mutex_unlock(&mutex_procesos);
					sem_post(&listos);
					pthread_mutex_unlock(&mutex_archivo);
					free(mensaje_planificador);
					break;
				}
			}

			if(string_equals_ignore_case(parametros[0],"escribir")){
				actual++;
				char * hola = armar_mensaje(parametros);
				int tam = string_length(hola);
				char * escribir = string_substring(hola, 1, tam-2);
				if(actual <= limite){
					pthread_mutex_lock(&mutex_procesos);
					time_t inicial = time(NULL);
					usleep(cpu->retardo*1000);
					char * mensaje = enviar_escribir_memoria(atoi(parametros[1]), escribir, contexto);
					sumar_tiempo_ejecucion(inicial, cpu_new);
					if(string_equals_ignore_case(mensaje, "1")){
						log_info(log_pantalla, "[CPU ID: %s] mProc PID: %d Escribio '%s' en la pagina %d",cpu_new->idCPU,
								contexto->pcb->pid, escribir, atoi(parametros[1]));
						string_append(&escribir, "|");
						string_append(&escribir, parametros[1]);
						mensaje_planificador = armado_mensajes_planificador("E", escribir, mensaje_planificador);
					}
					else{
						char * mensaje_iniciar_plan = string_new();
						string_append(&mensaje_iniciar_plan ,"I&");
						string_append(&mensaje_iniciar_plan ,serializar_pcb(contexto->pcb));
						string_append(&mensaje_iniciar_plan ,"&");
						string_append(&mensaje_iniciar_plan ,"2");
						string_append(&mensaje_iniciar_plan ,"&");
						string_append(&mensaje_iniciar_plan ,string_itoa(contexto->pcb->cant_var_cont_actual));
						int32_t socket_plan2 = crear_socket_cliente_conectado(cpu->ip_planificador,
									string_itoa(cpu->puerto_planificador));
						enviar_string(socket_plan2, mensaje_iniciar_plan);
						log_error(log_pantalla, "[CPU ID: %s] mProc PID: %d Error al escribir '%s' en la pagina: %d, abortando proceso",
								cpu_new->idCPU, contexto->pcb->pid, escribir, atoi(parametros[1]));
						pthread_mutex_unlock(&mutex_procesos);
						sem_post(&listos);
						pthread_mutex_unlock(&mutex_archivo);
						free(mensaje_iniciar_plan);
						break;
					}
					pthread_mutex_unlock(&mutex_procesos);
				}
				else{
					actual = 0;
					pthread_mutex_lock(&mutex_procesos);
					enviar_fin_quantum(contexto, j, cpu_new->idCPU, socket, mensaje_planificador);
					pthread_mutex_unlock(&mutex_procesos);
					sem_post(&listos);
					pthread_mutex_unlock(&mutex_archivo);
					free(mensaje_planificador);
					break;
				}
				free(escribir);
				tam = 0;
			}

			if(string_equals_ignore_case(parametros[0],"finalizar")){
				pthread_mutex_lock(&mutex_procesos);
				enviar_finalizar_memoria(contexto, cpu_new->idCPU, mensaje_planificador);
				pthread_mutex_unlock(&mutex_procesos);
				sem_post(&listos);
				pthread_mutex_unlock(&mutex_archivo);
				free(mensaje_planificador);
				break;
			}

		}
		else{
			if(string_equals_ignore_case(parametros[0],"entrada-salida")){
				actual = 0;
				pthread_mutex_lock(&mutex_procesos);
				usleep(cpu->retardo*1000);
				enviar_entrada_salida(atoi(parametros[1]), contexto, cpu_new->idCPU, socket, j, mensaje_planificador);
				puts("--------------------------------------------------------------------------------------------------------");
				log_info(log_pantalla, "[CPU ID: %s] mProc PID: %d Envia una petición de E/S de tiempo :%s",
						cpu_new->idCPU, contexto->pcb->pid, parametros[1]);
				puts("--------------------------------------------------------------------------------------------------------");
				pthread_mutex_unlock(&mutex_procesos);
				sem_post(&listos);
				pthread_mutex_unlock(&mutex_archivo);
				break;
			}
		}
	}
	free(rafagas);
	return 1;
}

void enviar_fin_quantum(context_ejec * contexto, int32_t pc, char * id_cpu, int32_t socket, char * sentencias){
	puts("-----------------------------------------------------------------------------------");
	log_info(log_pantalla, "[CPU ID: %s] mProc PID: %d, sale por límite de quantum : %d.",id_cpu, contexto->pcb->pid, contexto->quantum);
	int32_t socket_plan = crear_socket_cliente_conectado(cpu->ip_planificador,
			string_itoa(cpu->puerto_planificador));
	puts("-----------------------------------------------------------------------------------");

	if(socket_plan == -1){
		log_error(log_cpu, "Error al conectar con el proceso Planificador");
		exit(EXIT_FAILURE);
	}

	char * mensaje_quantum = string_new();
	string_append(&mensaje_quantum,"Q&");
	string_append(&mensaje_quantum,id_cpu);
	string_append(&mensaje_quantum,"&");
	string_append(&mensaje_quantum,serializar_pcb(contexto->pcb));
	string_append(&mensaje_quantum,"&");
	string_append(&mensaje_quantum,string_itoa(pc));
	string_append(&mensaje_quantum,"&");
	string_append(&mensaje_quantum, sentencias);
	string_append(&mensaje_quantum, "F");

	//Envio mensaje al planificador
	enviar_string(socket_plan, mensaje_quantum);
	free(mensaje_quantum);
}

int enviar_iniciar_memoria(int32_t cant_paginas, context_ejec * contexto, char * id_cpu){
	int32_t socket_plan = crear_socket_cliente_conectado(cpu->ip_planificador,
			string_itoa(cpu->puerto_planificador));

	int32_t socket_memoria = crear_socket_cliente_conectado(cpu->ip_memoria,
			string_itoa(cpu->puerto_memoria));

	if(socket_memoria == -1){
		log_error(log_cpu, "Error al conectar con el proceso Memoria");
		exit(EXIT_FAILURE);
	}

	char * mensaje_iniciar = string_new();
	string_append(&mensaje_iniciar,"1|");
	string_append(&mensaje_iniciar,string_itoa(contexto->pcb->pid));
	string_append(&mensaje_iniciar,"|");
	string_append(&mensaje_iniciar,string_itoa(cant_paginas));

	//Envio mensaje a memoria
	usleep(cpu->retardo*1000);
	enviar_string(socket_memoria, mensaje_iniciar);
	free(mensaje_iniciar);
	char * mensaje_recibido = recibir_y_deserializar_mensaje(socket_memoria);
	char * mensaje_iniciar_plan = string_new();

	if(atoi(mensaje_recibido) == 1){
		string_append(&mensaje_iniciar_plan ,"I&");
		string_append(&mensaje_iniciar_plan ,serializar_pcb(contexto->pcb));
		string_append(&mensaje_iniciar_plan ,"&");
		string_append(&mensaje_iniciar_plan ,"1");
		string_append(&mensaje_iniciar_plan ,"&");
		string_append(&mensaje_iniciar_plan ,string_itoa(contexto->pcb->cant_var_cont_actual));
		enviar_string(socket_plan, mensaje_iniciar_plan);
		log_info(log_pantalla, "[CPU ID: %s] mProc PID: %d inició exitosamente %d páginas",
				id_cpu, contexto->pcb->pid, cant_paginas);
	}
	else{
		string_append(&mensaje_iniciar_plan ,"I&");
		string_append(&mensaje_iniciar_plan ,serializar_pcb(contexto->pcb));
		string_append(&mensaje_iniciar_plan ,"&");
		string_append(&mensaje_iniciar_plan ,"0");
		string_append(&mensaje_iniciar_plan ,"&");
		string_append(&mensaje_iniciar_plan ,string_itoa(contexto->pcb->cant_var_cont_actual));
		enviar_string(socket_plan, mensaje_iniciar_plan);
	}
	free(mensaje_iniciar_plan);
	return atoi(mensaje_recibido);
}

char * enviar_leer_memoria(int32_t nro_paginas, context_ejec * contexto){

	int32_t socket_memoria = crear_socket_cliente_conectado(cpu->ip_memoria,
											string_itoa(cpu->puerto_memoria));

	if(socket_memoria == -1){
			log_error(log_cpu, "Error al conectar con el proceso Memoria");
			exit(EXIT_FAILURE);
	}

	char * mensaje_leer = string_new();
	string_append(&mensaje_leer,"3|");
	string_append(&mensaje_leer,string_itoa(contexto->pcb->pid));
	string_append(&mensaje_leer,"|");
	string_append(&mensaje_leer,string_itoa(nro_paginas));

	//Envio mensaje a memoria
	enviar_string(socket_memoria, mensaje_leer);
	free(mensaje_leer);
	char * mensaje_recibido = recibir_y_deserializar_mensaje(socket_memoria);
	return mensaje_recibido;
}

char * enviar_escribir_memoria(int32_t nro_paginas, char * texto, context_ejec * contexto){

	int32_t socket_memoria = crear_socket_cliente_conectado(cpu->ip_memoria,
											string_itoa(cpu->puerto_memoria));

	if(socket_memoria == -1){
			log_error(log_cpu, "Error al conectar con el proceso Memoria");
			exit(EXIT_FAILURE);
	}

	char * mensaje_escribir = string_new();
	string_append(&mensaje_escribir,"2|");
	string_append(&mensaje_escribir,string_itoa(contexto->pcb->pid));
	string_append(&mensaje_escribir,"|");
	string_append(&mensaje_escribir,string_itoa(nro_paginas));
	string_append(&mensaje_escribir,"|");
	string_append(&mensaje_escribir,texto);

	//Envio mensaje a memoria
	enviar_string(socket_memoria, mensaje_escribir);
	free(mensaje_escribir);
	char * mensaje_recibido = recibir_y_deserializar_mensaje(socket_memoria);
	return mensaje_recibido;
}

void enviar_entrada_salida(int32_t tiempo, context_ejec * contexto, char * id_cpu, int32_t socket, int32_t program, char * sentencias){

	int32_t socket_plan = crear_socket_cliente_conectado(cpu->ip_planificador,
			string_itoa(cpu->puerto_planificador));

	if(socket_plan == -1){
		log_error(log_cpu, "Error al conectar con el proceso Planificador");
		exit(EXIT_FAILURE);
	}

	char * mensaje_entrada_salida = string_new();
	string_append(&mensaje_entrada_salida,"E&");
	string_append(&mensaje_entrada_salida,id_cpu);
	string_append(&mensaje_entrada_salida,"&");
	string_append(&mensaje_entrada_salida,string_itoa(tiempo));
	string_append(&mensaje_entrada_salida,"&");
	string_append(&mensaje_entrada_salida,serializar_pcb(contexto->pcb));
	string_append(&mensaje_entrada_salida,"&");
	string_append(&mensaje_entrada_salida,string_itoa(program));
	string_append(&mensaje_entrada_salida,"&");
	string_append(&mensaje_entrada_salida, sentencias);
	string_append(&mensaje_entrada_salida, "F");

	//Envio mensaje al planificador
	enviar_string(socket_plan, mensaje_entrada_salida);
	free(mensaje_entrada_salida);
 }

void enviar_finalizar_memoria(context_ejec * contexto, char * id_cpu, char * sentencias){

	int32_t socket_p = crear_socket_cliente_conectado(cpu->ip_planificador,
											string_itoa(cpu->puerto_planificador));
	int32_t socket_m = crear_socket_cliente_conectado(cpu->ip_memoria,
										string_itoa(cpu->puerto_memoria));
	if(socket_m == -1){
		log_error(log_cpu, "Error al conectar con el proceso Memoria");
		exit(EXIT_FAILURE);
	}
	if(socket_p == -1){
		log_error(log_cpu, "Error al conectar con el proceso PLanificador");
		exit(EXIT_FAILURE);
	}

	char * mensaje_finalizar = string_new();
	string_append(&mensaje_finalizar,"5&");
	string_append(&mensaje_finalizar,id_cpu);
	string_append(&mensaje_finalizar,"&");
	string_append(&mensaje_finalizar, serializar_pcb(contexto->pcb));
	string_append(&mensaje_finalizar,"&");
	string_append(&mensaje_finalizar, sentencias);
	string_append(&mensaje_finalizar, "F");

	char * mensaje_finalizar_memoria = string_new();
	char ** msjSpliteado = string_split(serializar_pcb(contexto->pcb), "|");
	string_append(&mensaje_finalizar_memoria,"5|");
	string_append(&mensaje_finalizar_memoria, msjSpliteado[2]);

	//Envio mensaje a memoria y a planificador
	usleep(cpu->retardo*1000);
	enviar_string(socket_m, mensaje_finalizar_memoria);
	enviar_string(socket_p, mensaje_finalizar);
	free(mensaje_finalizar);
	free(mensaje_finalizar_memoria);
	free(msjSpliteado);

	puts("-----------------------------------------------------------------------------------");
	log_info(log_pantalla, "[CPU ID: %s] Finaliza el mProc PID : %d ",id_cpu, contexto->pcb->pid);
	puts("-----------------------------------------------------------------------------------");
}
