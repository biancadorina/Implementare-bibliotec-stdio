// #include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "so_stdio.h"
#include <errno.h>
#include <stdio.h>

#define SEEK_SET 0 /* Seek from beginning of file.  */
#define SEEK_CUR 1 /* Seek from current position.  */
#define SEEK_END 2 /* Seek from end of file.  */

#define SO_EOF (-1)
#define BUFFER 4096

typedef struct _so_file {
	int f_descriptor;
	int f_position;
	int err;
	char buffer[BUFFER];
	int buff_position;
	int buff_read;
	int buff_write;
	int f_fflush;
	int f_last_write;
	int f_last_read;
	int app;

} SO_FILE;

int so_fseek(SO_FILE *stream, long offset, int whence)
{
	if (stream->f_descriptor == -1)
		return SO_EOF;
	if (stream->f_last_write) {
		int f = so_fflush(stream);

		if (f == SO_EOF)
			return SO_EOF;
	}

	off_t poz = lseek(stream->f_descriptor, offset, whence);

	if (poz == -1) {
		stream->err = 1;
		return SO_EOF;
	}

	stream->f_position = poz;
	stream->buff_position = 0;
	stream->buff_read = 0;

	return 0;
}

SO_FILE *so_fopen(const char *pathname, const char *mode)
{
	SO_FILE *fis = (SO_FILE *)malloc(sizeof(SO_FILE));

	if (fis == NULL)
		return NULL;

	fis->f_position = 0;
	fis->buff_position = 0;
	fis->buff_read = 0;
	fis->buff_write = 0;
	fis->f_fflush = 0;
	fis->f_last_write = 0;
	fis->f_last_read = 0;
	fis->app = 0;
	fis->err = 0;

	if (!strcmp(mode, "r")) {
		fis->f_descriptor = open(pathname, O_RDONLY);
	} else if (!strcmp(mode, "r+")) {
		fis->f_descriptor = open(pathname, O_RDWR);
	} else if (!strcmp(mode, "w")) {
		fis->f_descriptor = open(pathname, O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP);
	} else if (!strcmp(mode, "w+")) {
		fis->f_descriptor = open(pathname, O_RDWR | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP);
	} else if (!strcmp(mode, "a")) {
		fis->f_descriptor = open(pathname, O_APPEND | O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP);
		fis->f_position = lseek(fis->f_descriptor, SEEK_END, 0);
	} else if (!strcmp(mode, "a+")) {
		fis->f_descriptor = open(pathname, O_APPEND | O_RDWR | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP);
		fis->f_position = lseek(fis->f_descriptor, SEEK_END, 0);
	} else {
		// fis->err = 1;
		free(fis);
		return NULL;
	}
	if (fis->f_descriptor == -1) {
		fis->err = 1;
		fis->f_descriptor = -1;
		free(fis);
		return NULL;
	}

	fis->f_fflush = 0;
	return fis;
}

int so_fclose(SO_FILE *stream)
{
	if (stream->f_descriptor == -1) {
		stream->err = 1;
		return SO_EOF;
	}

	if (stream->f_last_write) {
		int flush_result = so_fflush(stream);

		if (flush_result == SO_EOF) {
			stream->err = 1;
			stream->buff_write = 0;
			return SO_EOF;
		}
	}

	if (close(stream->f_descriptor) == -1) {
		stream->err = 1;
		perror("Eroare la inchidere");
		return SO_EOF;
	}
	free(stream);
	return 0;
}

int so_fgetc(SO_FILE *stream)
{
	stream->f_last_read = 1;
	stream->f_last_write = 0;

	if (stream->buff_position == 0 || stream->buff_position == stream->buff_read) {
		ssize_t bytes = read(stream->f_descriptor, stream->buffer, BUFFER);

		if (bytes == -1) {
			stream->err = 1;
			perror("eroare citire\n");
			return SO_EOF;
		}
		if (bytes == 0)
			return SO_EOF;
		stream->buff_read = bytes;
		stream->buff_position = 0;
		stream->f_position = bytes;
	}

	return stream->buffer[stream->buff_position++];
}

size_t so_fread(void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
	size_t elemente = size * nmemb;
	size_t nr_elemente = 0;

	for (size_t i = 0; i < elemente; i++) {
		int ch = so_fgetc(stream);

		((char *)ptr)[i] = ch;
		nr_elemente++;
	}
	return nr_elemente/size;
}

int so_fputc(int c, SO_FILE *stream)
{
	stream->f_last_read = 0;
	stream->f_last_write = 1;
	if (stream == NULL) {
		stream->err = 1;
		return SO_EOF;
	}

	if (stream->buff_position == BUFFER) {
		int f = so_fflush(stream);

		if (f == SO_EOF) {
			stream->err = 1;
			return SO_EOF;
		}
	}

	stream->buffer[stream->buff_position] = c;
	stream->buff_position++;

	stream->f_last_write = 1;

	return (unsigned char)c;
}

size_t so_fwrite(const void *ptr, size_t size, size_t nmemb, SO_FILE *stream)
{
	size_t elemente = size * nmemb;
	size_t nr_elem = 0;

	for (size_t i = 0; i < elemente; i++) {
		if (so_fputc(((const char *)ptr)[i], stream) == SO_EOF) {
			stream->err = 1;
			return 0;
		}
		nr_elem++;
	}

	return nr_elem/size;
}

long so_ftell(SO_FILE *stream)
{
	if (stream == NULL)
		return SO_EOF;
	if (stream->f_last_write) {
		int f = so_fflush(stream);

		if (f == SO_EOF)
			return SO_EOF;
	}
	//int d = lseek(stream->f_descriptor, SEEK_END, 0);

	return stream->f_position + stream->buff_position;
}

int so_fileno(SO_FILE *stream)
{
	return stream->f_descriptor;
}

int so_feof(SO_FILE *stream)
{
	if (stream->f_descriptor == -1) {
		stream->err = 1;
		return 0;
	}
	int val = lseek(stream->f_descriptor, 0, SEEK_END);

	return 1;
}

int so_ferror(SO_FILE *stream)
{
	if (stream->err == 1)
		return stream->err;
	else
		return 0;
}

int so_fflush(SO_FILE *stream)
{
	if (stream->buff_position > 0) {
		int bytes = write(stream->f_descriptor, stream->buffer, stream->buff_position);

		stream->f_position += bytes;
		if (bytes == -1) {
			stream->err = 1;
			perror("eroare scriere\n");
			return SO_EOF;
		}
		stream->buff_write = stream->buff_position;
		stream->buff_position = 0;
	}
	stream->f_fflush = 0;
	stream->buff_position = 0;
	stream->buff_read = 0;
	return 0;
}

SO_FILE *so_popen(const char *command, const char *type)
{
	pid_t pid;

	pid = fork();
	if (pid == -1)
		return NULL;
}
