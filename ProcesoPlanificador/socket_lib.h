#ifndef SRC_SOCKET_H_
#define SRC_SOCKET_H_

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <commons/string.h>
#include <sys/epoll.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h> //Para que pueda cambiar el flag de un socket a non_blocking
#include <unistd.h>
#include <commons/collections/list.h>

#define CANTIDAD_CONEXIONES_LISTEN 20


typedef struct{
	u_int32_t lengthName;
	char* fileName;
} fileName_t;

typedef struct sock{
	int32_t fd;
	struct sockaddr_in* sockaddr;
} sock_t;

typedef struct paquete{
	int32_t size;
	void* content;
}package_t;



/* FUNCIONES PRIVADAS */

/*** Crea un Socket*/
sock_t* _create_socket();

/**
 * Prepara la conexion para trabajar con el socket.
 * Si es localhost usar NULL en la IP
 */
void _prepare_conexion(sock_t*, char*, uint32_t);

/**
 * Limpia el puerto para reutilizarlo (para los casos en que se corre
 * varias veces un bind sobre el mismo puerto)
 */
int32_t _refresh_port(sock_t*);

/**
 * Bindea el puerto al Socket
 */
int32_t _bind_port(sock_t*);

/**
 * Envia un mensaje a traves del socket
 * Devuelve la cantidad de bytes que se enviaron realmente, o -1 en caso de error
 */
int32_t _send(sock_t*, void*, uint32_t);

/**
 * Lee un mensaje y lo almacena en buff
 * Devuelve la cantidad leia de bytes, 0 en caso de que la conexion este cerrada, o -1 en caso de error
 */
int32_t _receive(sock_t*, void*, uint32_t);

/*
 * Procedemos a cerrar el socket con la opcion SHUT_RDWR (No more receptions or transmissions)
 */
void _close_socket(sock_t* );

/**
 * Liberamos la memoria ocupada por el struct
 */
void _free_socket(sock_t*);

/*
 * Crea un paquete con el codigo dado y le agrega su tamaño
 */
package_t* _create_package(void*);

/*
 * Funcion que me retorna el tamaño standard de un paquete (tamaño mas codigo)
 */
int32_t _package_size(package_t*);

/*
 * Serializa un paquete, retorna el tamaño y su contenido compactado
 */
void* _serialize_package(package_t*);

/*
 * Deserializa un mensaje obteniendo el contenido y su correspondiente tamaño y los retorna en un paquete
 */
package_t* _deserialize_message(char*);


/**
 * Trata de enviar el mensaje completo, aunque este sea muy grande.
 * Deja en len la cantidad de bytes NO enviados
 * Devuelve 0 en caso de exito, o -1 si falla
 */
int32_t _send_bytes(sock_t*, void*, uint32_t);


/**
 * Trata de recibir el mensaje completo, aunque este se envie en varias rafagas.
 * Deja en len la cantidad de bytes NO recibidos
 * Devuelve 0 en caso de exito; o -1 si falla
 */
int32_t _receive_bytes(sock_t*, void*, uint32_t);


/*** FUNCIONES PUBLICAS ***/

/**
 * Crea un socket para recibir mensajes.
 * Recibe el puerto de la maquina remota que se va a conectar.
 * NO REALIZA EL LISTEN
 * @RETURNS: El struct socket.
 */
sock_t* create_server_socket(uint32_t);

/**
 * Crea un socket para enviar mensajes.
 * Recibe el puerto de la maquina remota que esta escuchando.
 * NO REALIZA EL CONNECT.
 *
 * @RETURNS: El struct socket.
 */
sock_t* create_client_socket(char*, uint32_t);

/**
 * Conecta con otra PC
 *
 * @RETURNS: -1 en caso de error
 * NOTA: Recordar que es una funcion bloqueante
 */
int32_t connect_to_server(sock_t*);

/**
 * Establece el socket para escuchar
 *
 *	NOTA: Esta funcion es NO bloqueante
 * @RETURNS: -1 en caso de error
 */
int32_t listen_connections(sock_t*);

/**
 * Acepta una conexion entrante
 *
 * @RETURNS: el nuevo FD; o NULL en caso de error
 * NOTA: Recordar que es una funcion bloqueante
 */
sock_t* accept_connection(sock_t*);

/*
 * Realiza el envio de un mensaje,
 * La funcion recibe el socket destino, y el mensaje (informacion) que se decia enviar
 */
int32_t send_msg(sock_t*, void*);

/*
 * Realiza el envio de una estructura compactada en memoria
 */
int32_t send_struct(sock_t*, void*, int32_t);

/*
 * Realiza la recepcion de un mensaje,
 * La funcion recibe el socket emisor del mensaje, y el buffer donde se colocara el mensaje recibido
 */
package_t* receive_msg(sock_t*, void*, int32_t);

/*
 * Realiza la recepcion de una estructra compactada en memoria
 * NOTA: debe tener el attribute_packed
 */
int32_t receive_struct(sock_t*, void*, int32_t);


/*
 * Cierra y libera el espacio ocupado por el socket en memoria
 */
void clean_socket(sock_t*);

char * recibir_y_deserializar_mensaje(int socket);
void itoa(int n, char s[]);
static void reverse(char s[]);
int enviar_string(int socket, char *mensaje);

#endif /* SOCKETS_H_ */



