#include <stdio.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>

// use this with write_converted_name, these
// macros are meant to clarify what one of the
// inputs actually do
#define IS_LOWER 0
#define IS_UPPER 1

// use this with trunc_extra_char, just like
// IS_LOWER and IS_UPPER, these exist for clarity
#define NO_ADD_EXTRA 0
#define ADD_EXTRA 1
#define ARRAY_SIZE(_array) \
	((int)(sizeof(_array) / sizeof((_array)[0])))

int is_dir(char *path) {
	if(path == NULL) {
		return 0;
	}
	
	struct stat path_stat = { 0 };
	if(stat(path, &path_stat) < 0) {
		return 0;
	}

	return S_ISDIR(path_stat.st_mode);
}

int is_empty(char *path) {
	if(path == NULL) {
		return 0;
	}
	
	struct stat path_stat = { 0 };
	errno = 0;
	if(stat(path, &path_stat) < 0) {

		// checks if path is empty string because
		// ENOENT is triggered when path is empty string
		// and also if the path doesnt exist
		if(errno == ENOENT && path[0] != 0) {
			return 1;
		}
	}

	return 0;
}

// doesnt handle errors, be sure to give good inputs
// to function
void write_converted_name(FILE *file, char *name, int is_upper) {
	int is_last_underscore = 0;
	int wrote_first_char = 0;
	for(int i = 0; name[i] != 0; i++) {
		if(isalpha(name[i])) {
			wrote_first_char = 1;
			is_last_underscore = 0;
			if(is_upper) {
				fputc(toupper(name[i]), file);
			} else {
				fputc(tolower(name[i]), file);
			}
			
			continue;
		}

		if(!wrote_first_char || is_last_underscore) {
			continue;
		}

		is_last_underscore = 1;
		fputc('_', file);
	}
}

// doesnt handle errors, be sure to give good inputs
// to function
void write_name_as_struct(FILE *file, char *name) {
	fprintf(file, "proto_");
	write_converted_name(file, name, IS_LOWER);
	fprintf(file, "_s");
}

int write_struct_decl(FILE *file, char *name, int index) {
	if(file == NULL || name == NULL) {
		return -1;
	}

	(void)(index);
	fprintf(file, "\tstruct ");
	write_name_as_struct(file, name);
	fprintf(file, ";\n");
	return 0;
}

int is_whitespace(char input) {
	return (
		input == ' ' ||
		input == '\n' ||
		input == '\r' ||
		input == '\t'
	);
}

// doesnt handle errors, be sure to give good inputs
// to function
void skip_whitespace(FILE *file) {
	if(feof(file)) {
		return;
	}
	
	char current = ' ';
	while(
		!feof(file) &&
		is_whitespace(current)
	) {
		current = fgetc(file);
	}

	// we will consume one non-whitespace character
	// so we go back one to cover for this
	if(!is_whitespace(current) && !feof(file)) {
		fseek(file, -1L, SEEK_CUR);
	}
}

// doesnt handle errors, be sure to give good inputs
// to function
#define STR_MAX 32
void read_to_whitespace(FILE *file, char *dest) {
	if(feof(file)) {
		return;
	}

	skip_whitespace(file);
	for(
		int i = 0; 
		i < STR_MAX &&
		!feof(file);
		i++
	) {
		dest[i] = fgetc(file);

		if(is_whitespace(dest[i])) {

			// this is a whitespace so it should end here anyway
			dest[i] = 0;
			break;
		}
	}
}

typedef struct {
	int is_na;
	char perm[STR_MAX];
} perm_t;

int read_perm(FILE *file, perm_t *perm) {
	if(file == NULL || perm == NULL) {
		return -1;
	}

	read_to_whitespace(file, (char *)(&perm->perm));
	perm->is_na = strcmp(perm->perm, "n/a") == 0;
	return 0;
}

typedef enum {
	PROP_TYPE_INT,
	PROP_TYPE_BUF,
	PROP_TYPE_CUSTOM
} prop_type_e;

typedef enum {
	PROP_CONN_NA,
	PROP_CONN_CLIENT,
	PROP_CONN_SERVER
} prop_conn_e;

typedef struct {
	char name[STR_MAX];
	prop_type_e type;
	char custom_type[STR_MAX];
	prop_conn_e conn;
} prop_t;

// doesnt handle errors, be sure to give good inputs
// to function
void read_prop_type(FILE *file, prop_t *prop) {
	char temp_type[STR_MAX] = { 0 };
	read_to_whitespace(file, (char *)(&temp_type));
	if(temp_type[0] == 0) {
		return;
	}
	
	prop_type_e types[] = {
		PROP_TYPE_INT,
		PROP_TYPE_BUF
	};

	char *type_names[] = {
		"int",
		"buf"
	};

	prop->type = PROP_TYPE_CUSTOM;
	for(int i = 0; i < ARRAY_SIZE(types); i++) {
		if(strcmp(temp_type, type_names[i]) == 0) {
			prop->type = types[i];
			break;
		}
	}

	// always copy to make it easy to
	// write types in other functions
	memcpy(
		prop->custom_type, 
		temp_type, 
		STR_MAX
	);
}

// doesnt handle errors, be sure to give good inputs
// to function
void read_prop_conn(FILE *file, prop_t *prop) {
	char temp_conn[STR_MAX] = { 0 };
	read_to_whitespace(file, (char *)(&temp_conn));
	if(temp_conn[0] == 0) {
		return;
	}
	
	prop_conn_e conns[] = {
		PROP_CONN_NA,
		PROP_CONN_CLIENT,
		PROP_CONN_SERVER
	};

	char *conn_names[] = {
		"n/a",
		"client",
		"server"
	};

	for(int i = 0; i < ARRAY_SIZE(conns); i++) {
		if(strcmp(temp_conn, conn_names[i]) == 0) {
			prop->conn = conns[i];
			break;
		}
	}
}

int read_prop(FILE *file, prop_t *prop) {
	if(file == NULL || prop == NULL) {
		return -1;
	}

	read_to_whitespace(file, (char *)(&prop->name));
	read_prop_type(file, prop);
	read_prop_conn(file, prop);
	return 0;
}

int trunc_extra_char(
	FILE *file, 
	char trunc_char, 
	int is_add_extra
) {
	if(file == NULL) {
		return -1;
	}

	if(fseek(file, -1L, SEEK_END) < 0) {
		printf("couldn't get to end of file!\n");
		perror("error");
		return -1;
	}
	
	off_t size = (off_t)(ftell(file)) + 1;
	if(size < 0) {
		printf("couldn't get file size!\n");
		perror("error");
		return -1;
	}

	char maybe_newline = trunc_char;
	while(
		maybe_newline == trunc_char && 
		size > 0 && 
		!feof(file)
	) {
		maybe_newline = fgetc(file);
		if(maybe_newline == trunc_char) {
			if(fseek(file, -2L, SEEK_CUR) < 0) {
				printf("couldn't move file cursor!\n");
				perror("error");
				return -1;
			}
			
			size--;
			if(
				ftruncate(
					fileno(file),
					size
				) < 0
			) {
				printf("couldn't truncate file!\n");
				perror("error");
				return -1;
			}
		}
	}

	if(is_add_extra) {
		fprintf(file, "%c", trunc_char);
	}

	return 0;
}

// this flag function is not smart, it will redefine flags
// that don't need to be defined, as to why the flag macros
// are macros in the first place and are wrapped inside of a #ifndef.
// as such, there will be bits that are never used, hopefully, though,
// this won't be a big problem because we have 64-bits (64 possible flags)
// to play with, and we can get away with wasting a few. if it does
// in fact become a problem i will find a way to fix it
int write_perm_flag_macro(FILE *file, char *name, int index) {
	if(file == NULL || name == NULL) {
		return -1;
	}

	FILE *proto_file = fopen(name, "r");
	if(proto_file == NULL) {
		return -1;
	}

	perm_t perm = { 0 };
	if(read_perm(proto_file, &perm) < 0) {
		fclose(proto_file);
		return -1;
	}
	
	if(strcmp(perm.perm, "n/a") == 0) {
		fclose(proto_file);
		return 0;
	}

	fclose(proto_file);
	fprintf(file, "\t#ifndef PROTO_PERM_");
	write_converted_name(file, perm.perm, IS_UPPER);
	fprintf(file, "\n\t\t#define PROTO_PERM_");
	write_converted_name(file, perm.perm, IS_UPPER);
	fprintf(file, " %d\n\t#endif\n\n", 1 << index);
	return 0;
}

int write_type_macro(FILE *file, char *name, int index) {
	if(file == NULL || name == NULL) {
		return -1;
	}
	
	fprintf(file, "\t#define PROTO_TYPE_");
	write_converted_name(file, name, IS_UPPER);
	fprintf(file, " %d\n", index);
	return 0;
}

int write_func_decl(FILE *file, char *name, int index) {
	if(file == NULL || name == NULL) {
		return -1;
	}

	(void)(index);
	fprintf(file, "\tint proto_");
	write_converted_name(file, name, IS_LOWER);
	fprintf(
		file, 
		"_update(void *, proto_inter_t *);\n"
		"\t#ifndef PROTO_IS_CLIENT\n"
		"\t\tint proto_"
	);
	
	write_converted_name(file, name, IS_LOWER);
	fprintf(
		file, 
		"_callback(void *, proto_inter_t *);\n"
		"\t#endif\n\n"
	);
	
	FILE *proto_file = fopen(name, "r");
	if(proto_file == NULL) {
		return -1;
	}

	perm_t skipped_perm = { 0 };
	if(read_perm(proto_file, &skipped_perm) < 0) {
		fclose(proto_file);
		return -1;
	}

	while(!feof(proto_file)) {
		prop_t prop = { 0 };
		if(read_prop(proto_file, &prop) < 0) {
			fclose(proto_file);
			return -1;
		}

		if(
			prop.type != PROP_TYPE_BUF || 
			prop.name[0] == 0
		) {
			continue;
		}

		fprintf(file, "\tint proto_");
		write_converted_name(file, name, IS_LOWER);
		fputc('_', file);
		write_converted_name(file, prop.name, IS_LOWER);
		fprintf(
			file, 
			"_buf_size(void *, int *);\n"
		);		
	}

	fclose(proto_file);
	return 0;
}

// can open too many files and fail if it 
// recurses too much
int write_inner_struct(FILE *file, char *name, char *type, int tab) {
	if(
		file == NULL || 
		name == NULL || 
		name[0] == 0 ||
		type == NULL ||
		type[0] == 0 ||
		tab < 0
	) {
		return -1;
	}
	
	for(int i = 0; i < tab; i++) {
		fputc('\t', file);
	}
	
	fprintf(file, "struct {\n");
	FILE *proto_file = fopen(type, "r");
	if(proto_file == NULL) {
		return -1;
	}
	
	perm_t skipped_perm = { 0 };
	if(read_perm(proto_file, &skipped_perm) < 0) {
		fclose(proto_file);
		return -1;
	}
	
	while(!feof(proto_file)) {
		prop_t prop = { 0 };
		if(read_prop(proto_file, &prop) < 0) {
			fclose(proto_file);
			return -1;
		}

		if(
			prop.type == PROP_TYPE_BUF || 
			prop.name == NULL ||
			prop.custom_type == NULL ||
			prop.name[0] == 0 ||
			prop.custom_type[0] == 0
		) {
			continue;
		}
		
		for(int i = 0; i < tab + 1; i++) {
			fputc('\t', file);
		}

		int is_custom = prop.type == PROP_TYPE_CUSTOM;
		if(is_custom) {
			if(
				write_inner_struct(
					file,
					prop.name,
					prop.custom_type,
					tab + 1
				) < 0
			) {
				return -1;
			}
			
			continue;
		}
		
		write_converted_name(file, prop.custom_type, IS_LOWER);
		fprintf(file, " ");
		write_converted_name(file, prop.name, IS_LOWER);
		fprintf(file, ";\n");
	}
	
	for(int i = 0; i < tab; i++) {
		fputc('\t', file);
	}
	
	fprintf(file, "} ");
	write_converted_name(file, name, IS_LOWER);
	fprintf(file, ";\n\n");
	fclose(proto_file);
	return 0;
}

int write_struct_impl(FILE *file, char *name, int index) {
	if(file == NULL || name == NULL) {
		return -1;
	}

	(void)(index);
	FILE *proto_file = fopen(name, "r");
	if(proto_file == NULL) {
		return -1;
	}

	perm_t skipped_perm = { 0 };
	if(read_perm(proto_file, &skipped_perm) < 0) {
		fclose(proto_file);
		return -1;
	}

	fprintf(file, "\ttypedef struct proto_");
	write_converted_name(file, name, IS_LOWER);
	fprintf(file, "_s {\n");
	while(!feof(proto_file)) {
		prop_t prop = { 0 };
		if(read_prop(proto_file, &prop) < 0) {
			fclose(proto_file);
			return -1;
		}

		if(
			prop.name == NULL ||
			prop.custom_type == NULL ||
			prop.name[0] == 0 ||
			prop.custom_type[0] == 0
		) {
			continue;
		}
		
		if(prop.type == PROP_TYPE_BUF) {
			fprintf(file, "\t\tvoid *");
			write_converted_name(file, prop.name, IS_LOWER);
			fprintf(file, ";\n");
			continue;
		}

		int is_custom = prop.type == PROP_TYPE_CUSTOM;
		if(is_custom) {
			if(
				write_inner_struct(
					file,
					prop.name,
					prop.custom_type,
					
					// 1 for being inside of an ifdef include guard.
					// 1 for being inside of a struct
					2
				) < 0
			) {
				return -1;
			}
			
			continue;
		}
		
		fprintf(file, "\t\t");
		write_converted_name(file, prop.custom_type, IS_LOWER);
		fprintf(file, " ");
		write_converted_name(file, prop.name, IS_LOWER);
		fprintf(file, ";\n");
	}

	trunc_extra_char(file, '\n', ADD_EXTRA);
	fprintf(file, "\t} proto_");
	write_converted_name(file, name, IS_LOWER);
	fprintf(file, "_t;\n\n");
	fclose(proto_file);
	return 0;
}

// doesnt handle errors, be sure to give good inputs
// to function
void write_func_calls(
	FILE *file, 
	FILE *proto_file,
	char *name, 
	prop_conn_e conn,
	int is_buf
) {
	rewind(proto_file);
	perm_t skipped_perm = { 0 };
	if(read_perm(proto_file, &skipped_perm) < 0) {
		fclose(proto_file);
		return;
	}

	while(!feof(proto_file)) {
		prop_t prop = { 0 };
		if(read_prop(proto_file, &prop) < 0) {
			fclose(proto_file);
			return;
		}

		if(
			(prop.type == PROP_TYPE_BUF && !is_buf) || 
			(prop.type != PROP_TYPE_BUF && is_buf) ||
			prop.conn != conn ||
			prop.name == NULL ||
			prop.custom_type == NULL ||
			prop.name[0] == 0 ||
			prop.custom_type[0] == 0
		) {
			continue;
		}

		fprintf(file, "\t\t\tproto_");
		if(prop.type == PROP_TYPE_BUF) {
			write_converted_name(file, name, IS_LOWER);
			fprintf(file, "_");
			write_converted_name(file, prop.name, IS_LOWER);
			fprintf(file, "_buf_size(");
			write_converted_name(file, name, IS_LOWER);
			fprintf(
				file, 
				", &temp_size) < 0 ||\n"
				"\t\t\tproto_buf_update(&"
			);
			
			write_converted_name(file, name, IS_LOWER);
			fprintf(file, "->");
			write_converted_name(file, prop.name, IS_LOWER);
			fprintf(file, ", temp_size, inter) < 0 ||\n");
			continue;
		}

		write_converted_name(file, prop.custom_type, IS_LOWER);
		fprintf(file, "_update(");
		fprintf(file, "(void *)(&");
		write_converted_name(file, name, IS_LOWER);
		fprintf(file, "->");
		write_converted_name(file, prop.name, IS_LOWER);
		fputc(')', file);
		fprintf(file, ", inter) < 0 ||\n");
	}
}

int write_func_impl(FILE *file, char *name, int index) {
	if(file == NULL || name == NULL) {
		return -1;
	}

	(void)(index);
	fprintf(file, "\tint proto_");
	write_converted_name(file, name, IS_LOWER);
	fprintf(
		file, 
		
		// we have to break convention here in order
		// to make sure this variable exists, because
		// names in proto folders must start with a letter
		// otherwise they are ignored
		"_update(void *_data, proto_inter_t *inter) {\n"
		"\t\tint temp_size = 0;\n"
		"\t\tint past_mode = 0;\n"
		"\t\tproto_"
	);
	
	write_converted_name(file, name, IS_LOWER);
	fprintf(file, "_t *");
	write_converted_name(file, name, IS_LOWER);
	fprintf(file, " = (proto_");
	write_converted_name(file, name, IS_LOWER);
	fprintf(
		file,
		"_t *)(_data);\n"
		"\t\treturn proto_err_check(\n"
		"\t\t\t"
	);
	
	write_converted_name(file, name, IS_LOWER);
	fprintf(
		file, 
		" == NULL ||\n"
		"\t\t\t!proto_inter_valid(inter) ||\n"
		"\t\t\tproto_inter_wrap(inter) < 0 ||\n"
		"\t\t\tproto_inter_copy_mode(inter, &past_mode) < 0 ||\n"
		"\t\t\tproto_inter_header(inter, PROTO_TYPE_"
	);
	
	write_converted_name(file, name, IS_UPPER);
	fprintf(file, ") < 0 ||\n");

	FILE *proto_file = fopen(name, "r");
	if(proto_file == NULL) {
		return -1;
	}

	perm_t perm = { 0 };
	if(read_perm(proto_file, &perm) < 0) {
		fclose(proto_file);
		return -1;
	}

	if(!perm.is_na) {
		fprintf(file, "\t\t\tproto_inter_try_perm(inter, PROTO_PERM_");
		write_converted_name(file, perm.perm, IS_UPPER);
		fprintf(file, ") < 0 ||\n");
	}

	prop_conn_e conns[] = {
		PROP_CONN_NA,
		PROP_CONN_CLIENT,
		PROP_CONN_SERVER
	};

	char *conn_names[] = {
		"CLIENT",
		"SERVER"
	};

	for(int i = 0; i < ARRAY_SIZE(conns); i++) {
		if(conns[i] == PROP_CONN_SERVER) {
			fprintf(
				file, 
				"\t\t\t#ifndef PROTO_IS_CLIENT\n"
				"\t\t\t\tproto_"
			);
			
			write_converted_name(file, name, IS_LOWER);
			fprintf(file, "_callback(");
			write_converted_name(file, name, IS_LOWER);
			fprintf(
				file, 
				", inter) < 0 ||\n"
				"\t\t\t#endif\n\n"
			);
		}
		
		if(conns[i] != PROP_CONN_NA && i <= 1) {
			fprintf(
				file, 
				"\t\t\tproto_inter_mode(inter, PROTO_INTER_%s) < 0 ||\n",

				// subtracted by one to account for the fact
				// that there is no name for PROP_CONN_NA in conn_names
				// list
				conn_names[i - 1]
			);
		}
		
		for(int is_buf = 0; is_buf <= 1; is_buf++) {
			write_func_calls(
				file, 
				proto_file, 
				name, 
				conns[i], 
				is_buf
			);
		}
	}
	
	fprintf(
		file,
		"\t\t\tproto_inter_mode(inter, past_mode) < 0 ||\n"
		"\t\t\tproto_inter_unwrap(inter) < 0\n"
		"\t\t);\n\t}\n\n"
	);
}

typedef int (*proto_file_write_f)(FILE *, char *, int);
int exec_with_proto_dir(FILE *file, proto_file_write_f proto_write) {
	if(file == NULL) {
		return -1;
	}
	
	DIR *dir = opendir(".");
	if(dir == NULL) {
		return -1;
	}

	int index = 0;
	struct dirent *entry = readdir(dir);
	while(entry != NULL) {
		if(!isalpha(entry->d_name[0])) {
			entry = readdir(dir);
			continue;
		}
		
		if(proto_write(file, entry->d_name, index) < 0) {
			closedir(dir);
			return -1;
		}
		
		entry = readdir(dir);
		index++;
	}

	return closedir(dir);
}

int write_header_start(FILE *file, char *path, int is_client) {
	if(file == NULL || path == NULL) {
		return -1;
	}
	
	fprintf(file, "#ifndef ");
	write_converted_name(file, path, IS_UPPER);
	fprintf(file, "\n#define ");
	write_converted_name(file, path, IS_UPPER);
	if(is_client) {
		fprintf(file, "\n\t#define PROTO_IS_CLIENT\n");
	}

	return 0;
}

// not text because while it's more simple it would turn into a
// maintainment nightmare
int file_append_file(FILE *to, char *from_path) {
	if(to == NULL || from_path == NULL) {
		return -1;
	}

	FILE *from = fopen(from_path, "r");
	if(from == NULL) {
		perror("error");
		return -1;
	}

	fputc('\t', to);
	while(!feof(from)) {
		char from_char = fgetc(from);
		if(feof(from)) {
			break;
		}
		
		fputc(from_char, to);
		
		// we check if we are at the end of the
		// file here because of an edge case in
		// how the file is generated. if we indent
		// the last line of the text then it makes the
		// line below it have an extra indent and that
		// looks weird
		if(from_char == '\n') {
			fputc('\t', to);
		}
	}

	// tidy up end of file by leaving
	// newline and no tabs after that newline
	trunc_extra_char(to, '\n', NO_ADD_EXTRA);
	trunc_extra_char(to, '\t', NO_ADD_EXTRA);
	fputc('\n', to);
	fclose(from);
	return 0;
}

int write_type_union(FILE *file) {
	if(file == NULL) {
		return -1;
	}
	
	DIR *dir = opendir(".");
	if(dir == NULL) {
		return -1;
	}
	
	fprintf(file, "\ttypedef union {\n");
	struct dirent *entry = readdir(dir);
	while(entry != NULL) {
		if(!isalpha(entry->d_name[0])) {
			entry = readdir(dir);
			continue;
		}
		
		fprintf(file, "\t\tproto_");
		write_converted_name(file, entry->d_name, IS_LOWER);
		fprintf(file, "_t ");
		write_converted_name(file, entry->d_name, IS_LOWER);
		fprintf(file, ";\n");
		entry = readdir(dir);
	}

	fprintf(file, "\t} proto_type_t;\n\n");
	return closedir(dir);
}

int write_update_funcs(FILE *file) {
	if(file == NULL) {
		return -1;
	}
	
	DIR *dir = opendir(".");
	if(dir == NULL) {
		return -1;
	}
	
	fprintf(file, "\tconst proto_update_f proto_update_funcs[] = {\n");
	struct dirent *entry = readdir(dir);
	int size = 0;
	while(entry != NULL) {
		if(!isalpha(entry->d_name[0])) {
			entry = readdir(dir);
			continue;
		}
		
		fprintf(file, "\t\tproto_");
		write_converted_name(file, entry->d_name, IS_LOWER);
		fprintf(file, "_update,\n");
		size++;
		entry = readdir(dir);
	}

	trunc_extra_char(file, '\n', NO_ADD_EXTRA);
	trunc_extra_char(file, ',', NO_ADD_EXTRA);
	fprintf(
		file, 
		"\n\t};\n\n"
		"\tconst int proto_update_func_size = %d;\n",
		size
	);
	
	return closedir(dir);
}

int main(int argc, char **argv) {

	// because sometimes when programs errors
	// out and prints something it doesnt show
	// because the buffer is not flushed. this
	// is a trick to get around the fact that we need
	// to flush the buffer, instead we just make it so 
	// we have no buffer to deal with at all
	setvbuf(stdout, NULL, _IONBF, 0);
	if(argc != 4) {
		printf(
			"usage: text-win-proto-gen"
			" config-dir gen-file "
			"--is-client/--is-server\n"
		);
		
		return 1;
	}

	if(!is_dir(argv[1])) {
		printf("config-dir isn't a directory!\n");
		return 1;
	}

	if(!is_empty(argv[2])) {
		printf("gen-file isn't empty!\n");
		return 1;
	}

	if(
		strcmp(argv[3], "--is-client") != 0 &&
		strcmp(argv[3], "--is-server") != 0
	) {
		printf("there is no --is-client/--is-server!\n");
		return 1;
	}

	FILE *output_file = fopen(argv[2], "w+");
	if(output_file == NULL) {
		printf("couldn't create the file to be generated!\n");
		perror("error");
		return 1;
	}

	int is_client = strcmp(argv[3], "--is-client") == 0;
	if(write_header_start(output_file, argv[2], is_client) < 0) {
		printf("couldn't write the start of the header file!\n");
		fclose(output_file);
		return 1;
	}

	if(file_append_file(output_file, "inter.h") < 0) {
		printf("couldn't add interface (inter) boilerplate!\n");
		fclose(output_file);
		return 1;
	}

	if(
		chdir(argv[1]) < 0 ||
		chdir("proto") < 0
	) {
		printf("couldn't start working on prototype directory!\n");
		perror("error");
		fclose(output_file);
		return 1;
	}

	// order matters here, don't change for aesthetic
	// reasons
	proto_file_write_f write_funcs[] = {
		write_perm_flag_macro,
		write_type_macro,
		write_struct_decl,
		write_func_decl,
		write_struct_impl,
		write_func_impl
	};

	for(int i = 0; i < ARRAY_SIZE(write_funcs); i++) {
		if(exec_with_proto_dir(output_file, write_funcs[i]) < 0) {
			printf("couldn't execute write function %d in directory!\n", i);
			fclose(output_file);
			return 1;
		}
	}
	
	// if more of these types of things are needed,
	// do another write func but with extra beginning and
	// end functions
	if(write_type_union(output_file) < 0) {
		printf("couldn't write type union!\n");
		fclose(output_file);
		return 1;
	}
	
	if(write_update_funcs(output_file) < 0) {
		printf("couldn't write update function array!\n");
		fclose(output_file);
		return 1;
	}

	// an edge case where we truncate newlines
	// to make file look better
	// keep one newline to not have #endif
	// to share a line with something else
	if(trunc_extra_char(output_file, '\n', ADD_EXTRA) < 0) {
		printf("couldn't cut newlines!\n");
		fclose(output_file);
		return -1;
	}

	fprintf(output_file, "#endif\n");
	fclose(output_file);
	return 0;
}
