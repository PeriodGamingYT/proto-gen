
// i need to make a small demo of client and server using a simple
// protocol to test the base stuff, then i can implement the text
// windows part of the text window manager, or just text-win
// no include guard because it's only meant to be added into
// a generated file by text-win-proto-gen
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>

// value to put into index in proto_inter_any_update
// because what uses the function, see more context
// in the comment for proto_inter_any_update
#define PROTO_NO_CONN -1

// malloc is slow and 64 should be enough for anyone.
#define PROTO_CONNS_MAX 64

// these macros decide the mode the inter is in
// if we are in PROTO_IS_CLIENT:
// 	PROTO_INTER_CLIENT means send stuff to the server
// 	PROTO_INTER_SERVER means read stuff from the server
//
// if we are NOT in PROTO_IS_CLIENT:
// 	PROTO_INTER_CLIENT means read stuff from the current client
// 	PROTO_INTER_SERVER means send stuff to the current client
#define PROTO_INTER_CLIENT 0
#define PROTO_INTER_SERVER 1

// message types that a client can send
// first one will ask for a permission
// the server will respond with a 1 for yes,
// and a 0 for no
#define PROTO_MESG_PERM 0

// this means the client wants itself to be
// cleaned up from the server and disconnected
// from it
#define PROTO_MESG_EXIT 1

// the client wants to send data with the server 
// the data is dependent on what protocol folder
// you auto-generate
#define PROTO_MESG_DATA 2

// since multiple data structures can be sent in one
// write from the client. we need to be able to tell
// if the next thing coming down the pipeline from a 
// client will include data or will not include any
// more data, hence why this is here
#define PROTO_IS_END 0
#define PROTO_IS_NOT_END 1

// 108 is the default nowdays, used here in pathname
// in order to prevent possible overflow by strlen()
#define PROTO_PATH_MAX 108

int proto_err_check(int expr) {

	// returns -1 if expr is 1, 0 otherwise:
	// for example here, expr = 5
	// (!!5) * -1
	// (!0) * -1
	// (1) * -1
	// 1 * -1
	// -1
	return (!!expr) * -1;
}

// to differentiate clients, we will have multiple clients
// be able to be accepted at a single time. next there
// are three things that can be sent to a server:
// the first one is a permission (0), it sends a 64-bit
// unsigned integer, which is or'ed into its flags, however
// this only happens when its function pointer (is_perm_okay_func)
// gives the okay to do so. 
// the next is leave (1), this just dismantles the connection on the server
// and cleans it up, nothing is sent, other than the leave instruction code.
// the next and most complex is proto (2), you send a number which corresponds
// to a data structure and then calls an update function on it. this is mainly
// what the protocol header generator is mostly for.
typedef struct {
	struct sockaddr_un addr;
	uint64_t perm_flags;
	socklen_t addr_size;
	int desc;

	// is_conn not apart of flags because
	// we want the user to have as many permission
	// flags as possible
	int is_conn;
} proto_conn_t;

// new flags (to be or'ed), server, and connection asking for perm
struct proto_server_s;
typedef int (*proto_try_perm_f)(
	uint64_t, 
	struct proto_server_s *, 
	int
);

typedef struct proto_server_s {
	proto_conn_t conns[PROTO_CONNS_MAX];
	struct sockaddr_un addr;
	int desc;
	proto_try_perm_f try_perm;
} proto_server_t;

typedef struct {
	struct sockaddr_un addr;
	int is_conn;
	int desc;
} proto_client_t;

// this exists because i am going to add more
// to this in the future, basically it's the main
// singleton "god" object of this program
typedef struct {

	// used in server main loop
	// so updates will talk to the
	// right client
	int conn_index;
	int is_client;
	int depth;
	int mode;
	union {
		proto_server_t server;
		proto_client_t client;
	} conn;
} proto_inter_t;

// since proto_path_t is automatically coerced into a normal string
// without adding extra padding, just plugging in a string literal
// will cause a segfault or worse.
// to mitigate this problem, do this:
// proto_path_t some_path = "path/to/socket";
// if(proto_inter_init(&inter, some_path, your_try_perm) < 0) {
// ...
//
// and not:
//
// // here the compiler will cohere proto_path_t without adding padding
// // which causes a segfault
// if(proto_inter_init(&inter, "path/to/socket", your_try_perm) < 0) {
// ...	
typedef char proto_path_t[PROTO_PATH_MAX];
int proto_server_init(
	proto_server_t *server, 
	proto_path_t pathname, 
	proto_try_perm_f try_perm
) {

	// make strlen() overflow impossible
	// by making sure the string is null
	// terminated
	pathname[PROTO_PATH_MAX - 1] = 0;
	if(
		server == NULL || 
		pathname == NULL || 
		strlen(pathname) >= sizeof(server->addr.sun_path) - 1 ||
		pathname[0] == 0
	) {
		return -1;
	}

	server->desc = socket(AF_UNIX, SOCK_STREAM, 0);
	if(server->desc < 0) {
		return -1;
	}

	server->addr.sun_family = AF_UNIX;
	int path_size = sizeof(server->addr.sun_path) - 1;
	strncpy(server->addr.sun_path, pathname, path_size);
	int yes = 1;
	if(
		setsockopt(
			server->desc,
			SOL_SOCKET,
			SO_REUSEADDR,
			&yes,
			sizeof(yes)
		) < 0
	) {
		close(server->desc);
		return -1;
	}

	if(
		bind(
			server->desc,
			(struct sockaddr *) &server->addr,
			sizeof(server->addr)
		) < 0
	) {
		close(server->desc);
		return -1;
	}

	if(listen(server->desc, PROTO_CONNS_MAX) < 0) {
		close(server->desc);
		return -1;
	}

	int fcntl_flags = fcntl(server->desc, F_GETFL, 0);
	if(fcntl_flags < 0) {
		close(server->desc);
		return -1;
	}

	if(
		fcntl(
			server->desc,
			F_SETFL,
			fcntl_flags | O_NONBLOCK
		) < 0
	) {
		close(server->desc);
		return -1;
	}

	server->try_perm = try_perm;
	return 0;
}

int proto_server_valid(proto_server_t *server) {
	return (
		server != NULL &&
		server->try_perm != NULL
	);
}

// will error if either server is invalid or
// there are no incoming connections, check that
// before erroring
int proto_server_init_conn(proto_server_t *server) {
	if(!proto_server_valid(server)) {
		return -1;
	}

	proto_conn_t *conn = NULL;
	int i = 0;
	for(i = 0; i < PROTO_CONNS_MAX; i++) {
		if(!server->conns[i].is_conn) {
			conn = &server->conns[i];
			break;
		}
	}

	if(conn == NULL) {
		return -1;
	}
	
	struct sockaddr_storage any_addr = { 0 };
	conn->addr_size = sizeof(any_addr);
	conn->desc = accept(
		server->desc,
		(struct sockaddr *) &any_addr,
		&conn->addr_size
	);

	if(conn->desc < 0) {
		return -1;
	}

	conn->addr = *(
		(struct sockaddr_un *)(&any_addr)
	);

	return i;
}

int proto_server_deinit_conn(proto_server_t *server, int index) {
	if(
		!proto_server_valid(server) ||
		index < 0 ||
		index >= PROTO_CONNS_MAX ||
		!server->conns[index].is_conn
	) {
		return -1;
	}

	proto_conn_t *conn = &server->conns[index];
	if(close(conn->desc) < 0) {
		return -1;
	}

	memset(
		conn, 
		0, 
		sizeof(proto_conn_t)
	);
	
	return 0;
}

int proto_server_write_conn(
	proto_server_t *server, 
	int index, 
	void *data, 
	int size
) {
	if(
		!proto_server_valid(server) ||
		index < 0 ||
		index >= PROTO_CONNS_MAX ||
		data == NULL ||
		size < 0
	) {
		return -1;
	}
	
	proto_conn_t *conn = &server->conns[index];
	if(write(conn->desc, data, size) < 0) {
		proto_server_deinit_conn(server, index);
		return -1;
	}
	
	return 0;
}

int proto_server_read_conn(
	proto_server_t *server, 
	int index, 
	void *data, 
	int size
) {
	if(
		!proto_server_valid(server) ||
		index < 0 ||
		index >= PROTO_CONNS_MAX ||
		data == NULL ||
		size < 0
	) {
		return -1;
	}
	
	proto_conn_t *conn = &server->conns[index];
	if(read(conn->desc, data, size) < 0) {
		proto_server_deinit_conn(server, index);
		return -1;
	}
	
	return 0;
}

int proto_server_deinit(proto_server_t *server) {
	if(server == NULL) {
		return -1;
	}

	struct sockaddr_un temp_addr = server->addr;
	char *temp_path = temp_addr.sun_path;
	for(int i = 0; i < PROTO_CONNS_MAX; i++) {
		if(!server->conns[i].is_conn) {
			continue;
		}
		
		if(proto_server_deinit_conn(server, i) < 0) {
			close(server->desc);
			unlink(temp_path);
			return -1;
		}
	}
	
	if(close(server->desc) < 0) {
		unlink(temp_path);
		return -1;
	}

	if(unlink(temp_path) < 0) {
		return -1;
	}
	
	memset(server, 0, sizeof(proto_server_t));
	return 0;
}

int proto_client_init(
	proto_client_t *client, 
	proto_path_t pathname
) {

	// make strlen() overflow impossible
	// by making sure the string is null
	// terminated
	pathname[PROTO_PATH_MAX - 1] = 0;
	if(
		client == NULL || 
		pathname == NULL || 
		strlen(pathname) >= sizeof(client->addr.sun_path) - 1 ||
		pathname[0] == 0
	) {
		return -1;
	}

	client->addr.sun_family = AF_UNIX;
	int path_size = sizeof(client->addr.sun_path) - 1;
	strncpy(
		client->addr.sun_path,
		pathname,
		path_size
	);

	client->is_conn = 1;
	int desc = socket(AF_UNIX, SOCK_STREAM, 0);
	if(desc < 0) {
		return -1;
	}

	if(
		connect(
			desc,
			(struct sockaddr *) &client->addr,
			sizeof(client->addr)
		) < 0
	) {
		close(desc);
		return -1;
	}

	client->desc = desc;
	return 0;
}

int proto_client_write(proto_client_t *client, void *data, int size) {
	if(
		client == NULL ||
		data == NULL ||
		size <= 0 ||
		!client->is_conn
	) {
		return -1;
	}

	if(write(client->desc, data, size) < 0) {
		close(client->desc);
		return -1;
	}

	return 0;
}

int proto_client_read(proto_client_t *client, void *data, int size) {
	if(
		client == NULL ||
		data == NULL ||
		size <= 0 ||
		!client->is_conn
	) {
		return -1;
	}

	if(read(client->desc, data, size) < 0) {
		close(client->desc);
		return -1;
	}

	return 0;
}

int proto_client_valid(proto_client_t *client) {
	return (
		client != NULL && (
			client->is_conn == 0 ||
			client->is_conn == 1
		)
	);
}

int proto_client_deinit(proto_client_t *client) {
	if(client == NULL) {
		return -1;
	}
	
	// don't bother setting is_conn to zero
	// because the whole client is going to 
	// be set to zero
	if(client->is_conn) {
		close(client->desc);
	}

	memset(client, 0, sizeof(proto_client_t));
	return 0;
}

int proto_inter_init(
	proto_inter_t *inter, 
	char *pathname, 
	proto_try_perm_f try_perm
) {

	// don't check pathname because the functions below will take
	// care of that
	if(inter == NULL) {
		return -1;
	}
	
	#ifdef PROTO_IS_CLIENT
		inter->is_client = 1;
		if(proto_client_init(&inter->conn.client, pathname) < 0) {
			return -1;
		}
	#else
		inter->is_client = 0;
		if(proto_server_init(&inter->conn.server, pathname, try_perm) < 0) {
			return -1;
		}
	#endif

	inter->conn_index = PROTO_NO_CONN;
	return 0;
}

int proto_inter_client_valid(proto_inter_t *inter) {
	return (
		inter != NULL && (
			inter->mode != PROTO_INTER_CLIENT || (
				inter->mode == PROTO_INTER_CLIENT &&
				proto_client_valid(&inter->conn.client)
			)
		)
	);
}

int proto_inter_server_valid(proto_inter_t *inter) {
	return (
		inter != NULL && (
			inter->mode != PROTO_INTER_SERVER || (
				inter->mode == PROTO_INTER_SERVER &&
				proto_server_valid(&inter->conn.server)
			)
		)
	);
}

int proto_inter_valid(proto_inter_t *inter) {
	return (
		inter != NULL && (
			inter->is_client == 0 ||
			inter->is_client == 1
		) &&
		
		inter->depth >= 0 && (
			inter->mode == PROTO_INTER_CLIENT ||
			inter->mode == PROTO_INTER_SERVER
		) && 
		
		proto_inter_client_valid(inter) &&
		proto_inter_server_valid(inter)
	);
}

int proto_inter_wrap(proto_inter_t *inter) {
	if(!proto_inter_valid(inter)) {
		return -1;
	}
	
	inter->depth++;
	return 0;
}

int proto_inter_unwrap(proto_inter_t *inter) {
	if(!proto_inter_valid(inter)) {
		return -1;
	}
	
	inter->depth--;
	return -1;
}

int proto_inter_copy_mode(proto_inter_t *inter, int *dest) {
	if(!proto_inter_valid(inter) || dest == NULL) {
		return -1;
	}
	
	*dest = inter->mode;
	return 0;
}

int proto_inter_mode(proto_inter_t *inter, int mode) {
	if(
		!proto_inter_valid(inter) ||
		mode < PROTO_INTER_CLIENT ||
		mode > PROTO_INTER_SERVER
	) {
		return -1;
	}
	
	// don't bounce between client and server.
	// just do one, and then the other, once
	if(inter->depth != 1) {
		return 0;
	}
	
	inter->mode = mode;
	return 0;
}

int proto_inter_any_update(
	proto_inter_t *inter, 
	void *data, 
	int size,
	
	// we break convention by adding index
	// at the end here, this is because this
	// function is used both by the server and 
	// client, the server uses the index and the
	// client does not. since one uses it we have
	// have to include it in here
	int index
) {
	if(
		!proto_inter_valid(inter) ||
		index < 0 ||
		index >= PROTO_CONNS_MAX ||
		data == NULL ||
		size < 0
	) {
		return -1;
	}
	
	#ifdef PROTO_IS_CLIENT
		(void)(index);
		if(inter->mode == PROTO_INTER_CLIENT) {
			return proto_client_write(
				&inter->conn.client,
				data,
				size
			);
		}
		
		return proto_client_read(
			&inter->conn.client,
			data,
			size
		);
	#else
		if(inter->mode == PROTO_INTER_CLIENT) {
			return proto_server_read_conn(
				&inter->conn.server,
				index,
				data,
				size
			);
		}
		
		return proto_server_write_conn(
			&inter->conn.server,
			index,
			data,
			size
		);
	#endif 
}

int proto_inter_header(proto_inter_t *inter, int type) {
	if(!proto_inter_valid(inter)) {
		return -1;
	}
	
	#ifdef PROTO_IS_CLIENT
		int header_data[] = {
			PROTO_MESG_DATA,
			PROTO_IS_NOT_END
		};
		
		return proto_client_write(
			&inter->conn.client,
			header_data,
			sizeof(header_data)
		);
	#else
		return 0;
	#endif
}

int proto_inter_finish(proto_inter_t *inter, int type) {
	if(!proto_inter_valid(inter)) {
		return -1;
	}
	
	#ifdef PROTO_IS_CLIENT
		int end_data[] = {
			PROTO_IS_END
		};
		
		return proto_client_write(
			&inter->conn.client,
			end_data,
			sizeof(end_data)
		);
	#else
		return 0;
	#endif
}

int proto_inter_try_perm(proto_inter_t *inter, uint64_t flags) {
	if(!proto_inter_valid(inter)) {
		return -1;
	}
	
	#ifdef PROTO_IS_CLIENT
		uint8_t perm_data[] = {
			PROTO_MESG_PERM,
			0, 
			0, 
			0, 
			0
		};
		
		*((uint64_t *)(&perm_data[1])) = flags;
		if(
			proto_client_write(
				&inter->conn.client,
				perm_data,
				sizeof(perm_data)
			) < 0
		) {
			return -1;
		}
		
		uint8_t is_accept = 0;
		if(
			proto_client_read(
				&inter->conn.client,
				&is_accept,
				sizeof(is_accept)
			) < 0
		) {
			return -1;
		}
		
		return is_accept;
	#else
		return 0;
	#endif
}

typedef int (*proto_update_f)(void *, proto_inter_t *);
const int proto_update_func_size;
const proto_update_f proto_update_funcs[];
int proto_inter_update_with(
	proto_inter_t *inter, 
	int index, 

	// the user can fill this in, typedef for this is auto-generated
	// and is called proto_type_t
	void *data
) {
	if(
		!proto_inter_valid(inter) ||
		index < 0 ||
		index >= PROTO_CONNS_MAX ||
		!inter->conn.server.conns[index].is_conn
	) {
		return -1;
	}
	
	inter->conn_index = index;
	proto_server_t *server = &inter->conn.server;
	proto_conn_t *conn = &server->conns[index];
	uint8_t code = 0;
	if(
		proto_server_read_conn(
			server, 
			index, 
			&code, 
			sizeof(code)
		) < 0
	) {
		proto_server_deinit_conn(server, index);
		return -1;
	}
	
	switch(code) {
		case PROTO_MESG_PERM: {
			uint64_t new_flag = 0;
			if(
				proto_server_read_conn(
					server, 
					index, 
					&new_flag, 
					sizeof(new_flag)
				) < 0
			) {
				proto_server_deinit_conn(server, index);
				return -1;
			}
			
			uint8_t accept_perm = 1;
			if(
				server->try_perm != NULL && 
				!server->try_perm(
					new_flag, 
					server, 
					index
				)
			) {
				accept_perm = 0;
			}
			
			if(
				proto_server_write_conn(
					server,
					index,
					&accept_perm,
					sizeof(accept_perm)
				) < 0
			) {
				proto_server_deinit_conn(server, index);
				return -1;
			}
			
			break;
		}
		
		case PROTO_MESG_EXIT: {
			if(proto_server_deinit_conn(server, index) < 0) {
				return -1;
			}
			
			break;
		}
		
		case PROTO_MESG_DATA: {
			uint8_t has_data = 1;
			if(
				proto_server_read_conn(
					server, 
					index, 
					&has_data, 
					sizeof(has_data)
				) < 0
			) {
				proto_server_deinit_conn(server, index);
				return -1;
			}
			
			if(!has_data) {
				break;
			}
			
			int type = 0;
			if(
				proto_server_read_conn(
					server, 
					index, 
					&type, 
					sizeof(type)
				) < 0
			) {
				proto_server_deinit_conn(server, index);
				return -1;
			}
			
			if(type < 0 || type >= proto_update_func_size) {
				proto_server_deinit_conn(server, index);
				return -1;
			}
			
			proto_update_funcs[type](data, inter);
			break;
		}
		
		default: {
			proto_server_deinit_conn(server, index);
			return -1;
		}
	}
	
	return 0;
}

int proto_inter_update(
	proto_inter_t *inter,
	
	// the user can fill this in, typedef for this is auto-generated
	// and is called proto_type_t
	void *data
) {
	if(
		!proto_inter_valid(inter) || 
		data == NULL
	) {
		return -1;
	}
	
	proto_server_t *server = &inter->conn.server;
	for(int i = 0; i < PROTO_CONNS_MAX; i++) {
		if(!server->conns[i].is_conn) {
			continue;
		}
		
		if(proto_inter_update_with(inter, i, data) < 0) {
			return -1;
		}
	}
	
	return 0;
}

int proto_inter_deinit(proto_inter_t *inter) {
	if(!proto_inter_valid(inter)) {
		return -1;
	}

	#ifdef PROTO_IS_CLIENT
		if(
			inter->is_client && 
			proto_client_deinit(&inter->conn.client) < 0
		) {
			return -1;
		}
	#else
		if(
			!inter->is_client && 
			proto_server_deinit(&inter->conn.server) < 0
		) {
			return -1;
		}
	#endif

	memset(inter, 0, sizeof(proto_inter_t));
	return 0;
}

// convention broken by having deinit function not be last.
// this is done for consistency with auto-generated code, which 
// has every single other data type updating function
int proto_int_update(int *dest, proto_inter_t *inter) {
	return proto_inter_any_update(
		inter,
		dest,
		sizeof(int),
		inter->conn_index
	);
}

int proto_buf_update(void **data, int size, proto_inter_t *inter) {
	if(!proto_inter_valid(inter)) {
		return -1;
	}
	
	// this code does allocate for buffers on the server
	// side, i didn't want to write this but in the end i
	// found no way out of this.
	// so, if you're on the server side, make sure to free
	// all the void * structs in a data type (those are buffers)
	// as long as they are not null.
	// only deal with this as long you're on the server and the
	// inter is in client mode
	#ifndef PROTO_IS_CLIENT
		if(inter->mode == PROTO_INTER_CLIENT) {
			*data = malloc(size);
			if(*data == NULL) {
				return -1;
			}
		}
	#endif
	
	return proto_inter_any_update(
		inter,
		*data,
		size,
		inter->conn_index
	);
}

// wrapper for free which handles null pointers
// for you, used as helper function in server code
void proto_free(void **ptr) {
	if(*ptr == NULL) {
		return;
	}
	
	// assumed to not fail
	free(*ptr);
	*ptr = NULL;
}