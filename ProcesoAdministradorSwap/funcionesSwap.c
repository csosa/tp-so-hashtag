#include "funcionesSwap.h"
#include "mensajeria.h"
#include <commons/collections/list.h>
#include <stdio.h>
#include <unistd.h>

swap* levantarConfig(char* path) {
	t_config* archConfig = config_create(path);
	swap* config = malloc(sizeof(swap));
	config->puerto_escucha = config_get_int_value(archConfig, "PUERTO_ESCUCHA");
	config->cantidad_paginas = config_get_int_value(archConfig,
			"CANTIDAD_PAGINAS");
	config->tamanio_pagina = config_get_int_value(archConfig, "TAMANIO_PAGINA");
	config->retardo_compactacion = config_get_int_value(archConfig,
			"RETARDO_COMPACTACION");
	config->retardo_swap = config_get_int_value(archConfig, "RETARDO_SWAP");

	/*Leo el nombre del archivo*/
	char* nombre = string_new();
	string_append(&nombre, config_get_string_value(archConfig, "NOMBRE_SWAP"));
	config->nombre_swap = nombre;

	config_destroy(archConfig);
	return config;
}
void iniciar_particion() {
	char* mej = string_new();
	u_int32_t tamanaio = archINICIO->tamanio_pagina
			* archINICIO->cantidad_paginas;
	char* stringTamnio = string_itoa(tamanaio);
	string_append(&mej, "dd if=/dev/zero of=");
	string_append(&mej, archINICIO->nombre_swap);
	string_append(&mej, " bs=");
	string_append(&mej, stringTamnio);
	string_append(&mej, " count=1");
	system(mej);
	free(stringTamnio);
	free(mej);
}

void rellenar_archivo_con_barra_cero() {
	u_int32_t i = 0;
	while (i < archINICIO->tamanio_pagina * archINICIO->cantidad_paginas) {
		fseek(particionSwap, i, SEEK_SET);
		char p = '\0';

		fputc(p, particionSwap);
		i++;
		free(p);
	}
}

void armar_lista_espacios_vacios() {
	estructura_vacia* espacioLibre = malloc(sizeof(estructura_vacia));
	espacioLibre->comienzo = fseek(particionSwap, 0, SEEK_SET);
	espacioLibre->cantPaginas = archINICIO->cantidad_paginas;
	list_add(espaciosLibres, espacioLibre);
}

void iniciar_comparticion_swap() {
	if (archINICIO->cantidad_paginas == 0 || archINICIO->tamanio_pagina == 0) {
		printf("No se ha cargado el tamaño de página o la cantidad\n");
		//colocarlo en un log
	} else {
		rellenar_archivo_con_barra_cero();
		armar_lista_espacios_vacios();
	}
}

void imprimo_por_log_la_asignacion(t_log* logSwap, u_int32_t posicion,
		u_int32_t pid, u_int32_t tamanio) {
	log_info(logSwap,
			"\nSe ASIGNA el proceso:\n n° de pid: %d\n byte inicial: %d\n byte final: %d\n\n",
			pid, posicion, posicion + (tamanio * archINICIO->tamanio_pagina));
}
void preparar_lista(u_int32_t posicion, u_int32_t cantidadDePaginasNecesarias,
		u_int32_t pid, t_log* logSwap) {
	u_int32_t iterator = 0;
	u_int32_t maximo = cantidadDePaginasNecesarias;
	u_int32_t inicio = posicion
			- (cantidadDePaginasNecesarias * archINICIO->tamanio_pagina);
	imprimo_por_log_la_asignacion(logSwap, inicio, pid,
			cantidadDePaginasNecesarias);
	while (iterator < maximo) {
		proceso* proceso = malloc(sizeof(proceso));
		proceso->pid = pid;
		proceso->posicion = posicion
				- (cantidadDePaginasNecesarias * archINICIO->tamanio_pagina);
		proceso->pagina = -1;
		list_add(procesosEnSwap, proceso);
		cantidadDePaginasNecesarias--;
		cantidaTotalDePaginasDisponibles--;
		iterator++;
	}
}

bool hay_espacio_contiguo(u_int32_t cantidadDePaginasNecesarias, u_int32_t pid,
		t_log* logSwap) {
	bool entra = false;
	u_int32_t iterator = 0;
	while (iterator < list_size(espaciosLibres)) {
		estructura_vacia* espacioVacio = list_get(espaciosLibres, iterator);
		if (espacioVacio->cantPaginas >= cantidadDePaginasNecesarias) {
			u_int32_t posicionFinal = (archINICIO->tamanio_pagina
					* cantidadDePaginasNecesarias) + espacioVacio->comienzo;
			preparar_lista(posicionFinal, cantidadDePaginasNecesarias, pid,
					logSwap);
			if ((espacioVacio->cantPaginas - cantidadDePaginasNecesarias)
					== 0) {
				list_remove(espaciosLibres, iterator);
				free(espacioVacio);
			} else {
				espacioVacio->comienzo = posicionFinal;
				espacioVacio->cantPaginas = espacioVacio->cantPaginas
						- cantidadDePaginasNecesarias;
			}
			entra = true;
		}
		iterator++;
	}
	return entra;
}

void reorganizar_tabla_procesos() {
	u_int32_t iterator = 0;
	u_int32_t posicion = 0;
	while (iterator < list_size(procesosEnSwap)) {
		proceso* proceso = list_get(procesosEnSwap, iterator);
		proceso->posicion = posicion;
		posicion = posicion + archINICIO->tamanio_pagina;
		iterator++;
	}
}
char* conseguir_dato(t_list* tabla, u_int32_t pid, u_int32_t pagina) {
	u_int32_t iterator = 0;
	while (iterator < list_size(tabla)) {
		dato* dato = list_get(tabla, iterator);
		if (dato->pagina == pagina && dato->pid == pid) {
			return dato->dato;
		} else {
			iterator++;
		}
	}
}
void escribir_procesos_en_swap(t_list* tablaOrganizativa) {
	u_int32_t indice = 0;
	while (indice < list_size(procesosEnSwap)) {
		proceso* datoNuevo = list_get(procesosEnSwap, indice);
		fseek(particionSwap, datoNuevo->posicion, SEEK_SET);
		if (datoNuevo->pagina != -1) {
			char* dato = conseguir_dato(tablaOrganizativa, datoNuevo->pid,
					datoNuevo->pagina);
			fputs(dato, particionSwap);
		}
		indice++;
	}
	free(tablaOrganizativa);
}

t_list* armar_tabla_organizativa() {
	particionSwap = fopen(archINICIO->nombre_swap, "r+");
	t_list* tablaOrganizativa = list_create();
	u_int32_t indice = 0;
	while (indice < list_size(procesosEnSwap)) {
		proceso* proceso = list_get(procesosEnSwap, indice);
		if (proceso->pagina != -1) {
			dato* datop = malloc(sizeof(dato));
			fseek(particionSwap, proceso->posicion, SEEK_SET);
			datop->dato = (char *) malloc(
					(proceso->tamanio + 1) * sizeof(char));
			fgets(datop->dato, proceso->tamanio + 1, particionSwap);
			datop->pagina = proceso->pagina;
			datop->pid = proceso->pid;
			list_add(tablaOrganizativa, datop);
		}
		indice++;
	}
	fclose(particionSwap);
	return tablaOrganizativa;
}

void rellenar_archivo_con_barra_cero_paralelo(FILE* archivo) {
	u_int32_t i = 0;
	while (i < archINICIO->tamanio_pagina * archINICIO->cantidad_paginas) {
		fseek(archivo, i, SEEK_SET);
		char p;
		fputc(p, archivo);
		i++;
		free(p);
	}
}

void compactar() {
	u_int32_t posicion = 0;
	nuevaParticion = fopen("prueba", "r+");
	rellenar_archivo_con_barra_cero_paralelo(nuevaParticion);
	if (nuevaParticion == NULL) {
		fputs("File error", stderr);
		exit(1);
	}
	u_int32_t indice = 0;
	while (indice < list_size(procesosEnSwap)) {
		proceso* proceso = list_get(procesosEnSwap, indice);
		if (proceso->pagina != -1) {
			fseek(particionSwap, proceso->posicion, SEEK_SET);
			char dato[proceso->tamanio + 1];
			fgets(dato, proceso->tamanio + 1, particionSwap);
			fseek(nuevaParticion, posicion, SEEK_SET);
			fputs(dato, nuevaParticion);
			posicion = posicion + archINICIO->tamanio_pagina;
		} else {
			vaciar(archINICIO->tamanio_pagina, posicion);
			posicion = posicion + archINICIO->tamanio_pagina;
		}
		indice++;
	}
	fclose(particionSwap);
	remove(particionSwap);
	rewind(nuevaParticion);
	rename("prueba", archINICIO->nombre_swap);
	particionSwap = nuevaParticion;
	u_int32_t posicionList = 0;
	indice = 0;
	while (indice < list_size(procesosEnSwap)) {
		proceso* proceso = list_get(procesosEnSwap, indice);
		proceso->posicion = posicionList;
		posicionList = posicionList + archINICIO->tamanio_pagina;
		indice++;
	}
	free(espaciosLibres);
	espaciosLibres = list_create();
	estructura_vacia* libre = malloc(sizeof(estructura_vacia));
	libre->comienzo = posicionList;
	libre->cantPaginas = cantidaTotalDePaginasDisponibles;
	list_add(espaciosLibres, libre);
}

void vaciar(u_int32_t tamanio, u_int32_t posicion) {
	u_int32_t i = 0;
	while (i < archINICIO->tamanio_pagina) {
		fseek(particionSwap, i + posicion, SEEK_SET);
		fputc(unico, particionSwap);
		i++;
	}
}

void logeo_de_liberacion(u_int32_t pid, t_log* logSwap) {
	u_int32_t iterator = 0;
	bool bandera = false;
	u_int32_t posicionInicial;
	u_int32_t contador = 0;
	while (iterator < list_size(procesosEnSwap)) {
		proceso* proceso = list_get(procesosEnSwap, iterator);
		if (proceso->pid == pid && bandera == false) {
			posicionInicial = proceso->posicion;
			bandera = true;
		}
		if (proceso->pid == pid) {
			contador++;
		}
		iterator++;
	}
	iterator = 0;
	u_int32_t cantidadPaginasEscritas = 0;
	if (list_size(paginasEscritasPorCadaProceso) != 0) {
		iterator = 0;
		while (iterator < list_size(paginasEscritasPorCadaProceso)) {
			datoDeLectura* escribir = list_get(paginasEscritasPorCadaProceso,
					iterator);
			if (escribir->pid == pid) {
				cantidadPaginasEscritas = escribir->cantidad;

			}
			iterator++;
		}
	}
	u_int32_t paginasLeidas = 0;
	if (list_size(paginasLeidasPorCadaProceso) != 0) {
		iterator = 0;
		while (iterator < list_size(paginasLeidasPorCadaProceso)) {
			datoDeLectura* lectura = list_get(paginasLeidasPorCadaProceso,
					iterator);
			if (lectura->pid == pid) {
				paginasLeidas = lectura->cantidad;

			}
			iterator++;
		}
	}
	u_int32_t final = posicionInicial + (archINICIO->tamanio_pagina * contador);
	log_info(logSwap,
			"\nSe LIBERA el proceso...\n n° de PID: %d \n n° de byte inicial: %d \n n° de byte "
					"final: %d \n esta es la cantidad total de paginas que han sido escritas por el proceso: %d\n "
					"esta es la cantidad total de paginas que han sido leidas por el proceso: %d\n",
			pid, posicionInicial, final, cantidadPaginasEscritas,
			paginasLeidas);

}
void res_destroy(proceso* self) {
	free(self);
}

void eliminar_paginas_escritas(u_int32_t pid) {
	bool elpid(proceso* proceso) {
		return proceso->pid != pid;
	}
	t_list* listMomentLeer = list_filter(paginasEscritasPorCadaProceso,
			(void*) elpid);

	list_clean(paginasEscritasPorCadaProceso);
	paginasEscritasPorCadaProceso = list_create();
	list_add_all(paginasEscritasPorCadaProceso, listMomentLeer);
}

void eliminar_paginas_leidas(u_int32_t pid) {
	bool elpid(proceso* proceso) {
		return proceso->pid != pid;
	}
	t_list* listMomentLeer = list_filter(paginasLeidasPorCadaProceso,
			(void*) elpid);

	list_clean(paginasLeidasPorCadaProceso);
	paginasLeidasPorCadaProceso = list_create();
	list_add_all(paginasLeidasPorCadaProceso, listMomentLeer);
}

u_int32_t cantidad_de_paginas_del_proceso( pid) {
	bool porElPid(proceso* proceso) {
		return proceso->pid == pid;
	}
	t_list* listCan = list_filter(procesosEnSwap, (void*) porElPid);
	u_int32_t cantidad = list_size(listCan);
	free(listCan);
	return cantidad;
}

u_int32_t incio_proceso( pid) {
	bool porElPid(proceso* proceso) {
		return proceso->pid == pid;
	}
	t_list* listCan = list_filter(procesosEnSwap, (void*) porElPid);

	bool porLaPosicion(proceso* proceso1, proceso* proceso2) {
		return proceso1->pid <= proceso2->pid;
	}
	list_sort(listCan, (void*) porLaPosicion);
	u_int32_t posicion = list_get(listCan, 0);
	free(listCan);
	return posicion;
}

bool coincide_final(u_int32_t pid) {
	u_int32_t iterator = 0;
	u_int32_t final = 0;
	bool termina = false;
	while (iterator < list_size(espaciosLibres) && termina != true) {
		estructura_vacia* vacio = list_get(espaciosLibres, iterator);
		final = vacio->comienzo
				+ vacio->cantPaginas * archINICIO->tamanio_pagina;
		u_int32_t indice = 0;
		while (indice < list_size(procesosEnSwap)) {
			proceso* proceso = list_get(procesosEnSwap, indice);
			if (proceso->posicion == final && pid == proceso->pid) {
				vacio->cantPaginas = vacio->cantPaginas
						+ cantidad_de_paginas_del_proceso(pid);
				termina = true;
			}
			indice++;
		}
		iterator++;
	}
	return termina;
}

bool coincide_comienzo( pid) {
	u_int32_t iterator = 0;
	u_int32_t final = 0;
	bool termina = false;
	while (iterator < list_size(espaciosLibres) && termina != true) {
		estructura_vacia* vacio = list_get(espaciosLibres, iterator);
		u_int32_t indice = 0;
		while (indice < list_size(procesosEnSwap)) {
			proceso* proceso = list_get(procesosEnSwap, indice);
			final = proceso->posicion + archINICIO->tamanio_pagina;
			if (vacio->comienzo == final && pid == proceso->pid) {
				vacio->comienzo = vacio->comienzo
						- (cantidad_de_paginas_del_proceso(pid)
								* archINICIO->tamanio_pagina);
				vacio->cantPaginas = vacio->cantPaginas
						+ cantidad_de_paginas_del_proceso(pid);
				termina = true;
			}
			indice++;
		}
		iterator++;
	}
	return termina;
}

void agrego_espacio_libre( pid) {
	estructura_vacia* vacio = malloc(sizeof(estructura_vacia));
	vacio->cantPaginas = cantidad_de_paginas_del_proceso(pid);
	vacio->comienzo = incio_proceso(pid);
	list_add(espaciosLibres, vacio);
}

void rearmo_lista_espacios_disponibles(u_int32_t pid) {
	if (!coincide_comienzo(pid) && !coincide_final(pid)) {
		agrego_espacio_libre(pid);
	}
}

void dar_de_baja(u_int32_t pid, t_log* logSwap) {
	logeo_de_liberacion(pid, logSwap);
	rearmo_lista_espacios_disponibles(pid);
	u_int32_t indice = 0;
	while (indice < list_size(procesosEnSwap)) {
		proceso* proceso = list_get(procesosEnSwap, indice);
		if (proceso->pid == pid && proceso->pagina != -1) {
			fseek(particionSwap, proceso->posicion, SEEK_SET);
			vaciar(proceso->tamanio, proceso->posicion);
		}
		indice++;
	}
	bool elpid(proceso* proceso) {
		return proceso->pid != pid;
	}
	t_list* listMoment = list_filter(procesosEnSwap, (void*) elpid);
	cantidaTotalDePaginasDisponibles = cantidaTotalDePaginasDisponibles
			+ (list_size(procesosEnSwap) - list_size(listMoment));
	free(procesosEnSwap);
	procesosEnSwap = list_create();
	list_add_all(procesosEnSwap, listMoment);
	eliminar_paginas_leidas(pid);
	eliminar_paginas_escritas(pid);
}

u_int32_t contar_paginas(u_int32_t pid) {
	u_int32_t iterator = 0;
	u_int32_t cantidad = 0;
	while (iterator < list_size(procesosEnSwap)) {
		proceso* proceso = list_get(procesosEnSwap, iterator);
		if (proceso->pid == pid) {
			cantidad++;
		}
		iterator++;
	}
	return cantidad;
}

void logeo_de_escritura(mensaje_peticion_escritura* mensaje) {
	u_int32_t iterator = 0;
	bool bandera = false;
	while (list_size(procesosEnSwap) > iterator && bandera == false) {
		proceso* proceso = list_get(procesosEnSwap, iterator);
		if (proceso->pid == mensaje->PID) {
			if (proceso->pagina == -1) {
				bandera = true;
				u_int32_t cantidadPaginas = contar_paginas(mensaje->PID);
			}
		}
		iterator++;
	}
}

void agrego_a_paginas_escritas( pid) {
	u_int32_t iterator = 0;
	bool finalizar = false;
	if (list_size(paginasEscritasPorCadaProceso) == 0) {
		datoDeLectura* escrituraDe = malloc(sizeof(datoDeLectura));
		escrituraDe->pid = pid;
		escrituraDe->cantidad = 1;
		list_add(paginasEscritasPorCadaProceso, escrituraDe);
	} else {
		while (iterator < list_size(paginasEscritasPorCadaProceso)
				&& finalizar == false) {
			datoDeLectura* dato = list_get(paginasEscritasPorCadaProceso,
					iterator);
			if (dato->pid == pid) {
				dato->cantidad = dato->cantidad + 1;
				finalizar = true;
			}
			iterator++;
		}
		if (finalizar == false) {
			datoDeLectura* lecturaDe = malloc(sizeof(datoDeLectura));
			lecturaDe->pid = pid;
			lecturaDe->cantidad = 1;
			list_add(paginasEscritasPorCadaProceso, lecturaDe);
		}
	}
}

void se_escribe_pagina(mensaje_peticion_escritura* mensaje, t_log* logSwap) {
	usleep(archINICIO->retardo_swap * 1000);
	u_int32_t iterator = 0;
	bool termina = false;
	logeo_de_escritura(mensaje);
	while (list_size(procesosEnSwap) > iterator) {
		proceso* proceso = list_get(procesosEnSwap, iterator);
		if (proceso->pid == mensaje->PID) {
			if (proceso->pagina == mensaje->pagina) {
				log_info(logSwap,
						"\nSe ESCRIBE el proceso... \n n° de PID: %d \n n° de byte inicial: %d \n"
								" n° de byte final: %d \n contenido: %s \n ",
						mensaje->PID, proceso->posicion,
						proceso->posicion + string_length(mensaje->texto),
						mensaje->texto);
				agrego_a_paginas_escritas(mensaje->PID);
				char* cadena = malloc(proceso->tamanio);
				u_int32_t i = 0;
				while (i < proceso->tamanio) {
					u_int32_t offset = strlen("\0");
					strcat(cadena + offset, "\0");
					i++;
				}
				fseek(particionSwap, 0, SEEK_SET);
				fseek(particionSwap, proceso->posicion, SEEK_CUR);
				char buffer[proceso->tamanio + 1];
				fread(buffer, proceso->tamanio + 1, 1, particionSwap);
				fseek(particionSwap, -sizeof(buffer), SEEK_CUR);
				fwrite(mensaje->texto, strlen(mensaje->texto) + 1, 1,
						particionSwap);
				proceso->tamanio = string_length(mensaje->texto);
				termina = true;
			}
		}
		iterator++;
	}
	if (termina == false) {
		iterator = 0;
		while (list_size(procesosEnSwap) > iterator && termina == false) {
			proceso* proceso = list_get(procesosEnSwap, iterator);
			if (proceso->pid == mensaje->PID) {
				if (proceso->pagina == -1) {
					log_info(logSwap,
							"\nSe ESCRIBE el proceso... \n n° de PID: %d \n n° de byte inicial: %d \n"
									" n° de byte final: %d \n contenido: %s \n ",
							mensaje->PID, proceso->posicion,
							proceso->posicion + string_length(mensaje->texto),
							mensaje->texto);
					agrego_a_paginas_escritas(mensaje->PID);
					char* cadena = malloc(proceso->tamanio);
					proceso->pagina = mensaje->pagina;
					proceso->tamanio = string_length(mensaje->texto);
					fseek(particionSwap, 0, SEEK_SET);
					fseek(particionSwap, proceso->posicion, SEEK_CUR);
					fputs(mensaje->texto, particionSwap);
					termina = true;
				}
			}
			iterator++;
		}
	}
	if (termina == false) {
		printf(
				"Quiere escribir una pagina que no ha sido inicializada para ese proceso\n");
	}
}

u_int32_t buscar_posicion_pagina(u_int32_t pid, u_int32_t pagina) {
	u_int32_t iterator = 0;
	u_int32_t posicion = -1;
	bool terminar = false;
	while (iterator < list_size(procesosEnSwap) && terminar == false) {
		proceso* proceso = list_get(procesosEnSwap, iterator);
		if (proceso->pid == pid && proceso->pagina == pagina) {
			posicion = proceso->posicion;
			terminar = true;
		}
		iterator++;
	}
	if (terminar == false) {
		//printf("No existe ese pid o esa pagina que queire escribir\n");
	}
	return posicion;
}

u_int32_t buscar_tamanio_contenido_pagina(u_int32_t pid, u_int32_t pagina) {
	u_int32_t iterator = 0;
	u_int32_t tamanio;
	bool terminar = false;
	while (iterator < list_size(procesosEnSwap) && terminar == false) {
		proceso* proceso = list_get(procesosEnSwap, iterator);
		if (proceso->pid == pid && proceso->pagina == pagina) {
			tamanio = proceso->tamanio;
			terminar = true;
		}
		iterator++;
	}
	return tamanio;
}

void agrego_a_paginas_leidas(u_int32_t pid) {
	u_int32_t iterator = 0;
	bool finalizar = false;
	if (list_size(paginasLeidasPorCadaProceso) == 0) {
		datoDeLectura* lecturaDe = malloc(sizeof(datoDeLectura));
		lecturaDe->pid = pid;
		lecturaDe->cantidad = 1;
		list_add(paginasLeidasPorCadaProceso, lecturaDe);
	} else {
		while (iterator < list_size(paginasLeidasPorCadaProceso)
				&& finalizar == false) {
			datoDeLectura* dato = list_get(paginasLeidasPorCadaProceso,
					iterator);
			if (dato->pid == pid) {
				dato->cantidad = dato->cantidad + 1;
				finalizar = true;
			}
			iterator++;
		}
		if (finalizar == false) {
			datoDeLectura* lecturaDe = malloc(sizeof(datoDeLectura));
			lecturaDe->pid = pid;
			lecturaDe->cantidad = 1;
			list_add(paginasLeidasPorCadaProceso, lecturaDe);
		}
	}
}

char* leer_pagina(mensaje_peticion_lectura* mensaje, t_log* logSwap) {
	usleep(archINICIO->retardo_swap * 1000);
	u_int32_t posicion = buscar_posicion_pagina(mensaje->PID, mensaje->pagina);
	agrego_a_paginas_leidas(mensaje->PID);
	if (posicion == -1) {
		return "por favor quiero que ande";
	} else {
		int32_t tamanioContenido = buscar_tamanio_contenido_pagina(mensaje->PID,
				mensaje->pagina);
		fseek(particionSwap, 0, SEEK_SET);
		fseek(particionSwap, posicion, SEEK_CUR);
		char* informacion;
		informacion = malloc(tamanioContenido + 1);
		fgets(informacion, tamanioContenido + 1, particionSwap);
		char p = fgetc(particionSwap);
		log_info(logSwap,
				"\nSe LEE el proceso... \n n° de PID: %d \n n° de byte inicial: %d \n n° de byte final: %d \n contenido: %s\n",
				mensaje->PID, posicion, posicion + tamanioContenido,
				informacion);
		return informacion;
	}
}

void borrar_contenido_de_pagina_de_tabla_procesos(u_int32_t posicion,
		u_int32_t PID) {
	u_int32_t indice = 0;
	bool terminar = false;
	while ((indice < list_size(procesosEnSwap)) && (terminar == false)) {
		proceso* proceso = list_get(procesosEnSwap, indice);
		if (proceso->pid == PID && proceso->posicion == posicion) {
			list_remove(procesosEnSwap, indice);

			terminar = true;
		}
		indice++;
	}
}

void borrar_pagina(u_int32_t posicion, u_int32_t PID) {
	fseek(particionSwap, 0, posicion);
	borrar_contenido_de_pagina_de_tabla_procesos(posicion, PID);
}
