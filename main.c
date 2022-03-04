/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * THIS HEADER MAY NOT BE EXTRACTED OR MODIFIED IN ANY WAY.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>

#include <sodium.h>

#define DEFAULT_LISTEN_PORT 12345

/* "shortcut" for struct sockaddr structure */
#define SSA			struct sockaddr

/* error printing macro */
#define ERR(call_description)                           \
	do {                                            \
		fprintf(stderr, "(%s, %d): %s\n",       \
				__FILE__, __LINE__,     \
				call_description);      \
	} while (0)

/* print error (call ERR) and exit */
#define DIE(assertion, call_description)                \
	do {                                            \
		if (assertion) {                        \
			ERR(call_description);          \
			return -1;                      \
		}                                       \
	} while(0)

/*
 * Create a server socket.
 */

static int tcp_create_listener(unsigned short port, int backlog)
{
	struct sockaddr_in address;
	int listenfd;
	int sock_opt;
	int rc;

	listenfd = socket(PF_INET, SOCK_STREAM, 0);
	DIE(listenfd < 0, "socket");

	sock_opt = 1;
	rc = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR,
			&sock_opt, sizeof(int));
	DIE(rc < 0, "setsockopt");

	memset(&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_port = htons(port);
	address.sin_addr.s_addr = INADDR_ANY;

	rc = bind(listenfd, (SSA *) &address, sizeof(address));
	DIE(rc < 0, "bind");

	rc = listen(listenfd, backlog);
	DIE(rc < 0, "listen");

	return listenfd;
}

/*
 * Use getpeername(2) to extract remote peer address. Fill buffer with
 * address format IP_address:port (e.g. 192.168.0.1:22).
 */

static int get_peer_address(int sockfd, char *buf, size_t __unused len)
{
	struct sockaddr_in addr;
	socklen_t addrlen = sizeof(struct sockaddr_in);

	if (getpeername(sockfd, (SSA *) &addr, &addrlen) < 0)
		return -1;

	sprintf(buf, "%s:%d", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));

	return 0;
}

/*
 * Process command filename to result of size rlen.
 */

static void process(const char *command_filename, char *result, size_t rlen)
{
	FILE *f;
	char buffer[256];
	enum {
		OP_NONE = 0,
		OP_ADD = 1,
		OP_SUB = 2,
		OP_HASH_GENERIC = 3,
		OP_HASH_SHA256 = 4,
		OP_HASH_SHA512 = 5,
		OP_EC_MULTIPLY = 6,
		OP_CORE_SCALAR_MULTIPLY = 7,
		OP_CORE_SCALAR_INVERT = 8,
	} op = OP_NONE;
	unsigned int num1, num2, res;

	f = fopen(command_filename, "rt");
	if (f == NULL) {
		printf("Error opening file %s\n", command_filename);
		sprintf(result, "No such file %s", command_filename);
		return;
	}

	/* Initialize result. */
	result[0] = '\0';

	/* Read operation. */
	fgets(buffer, 256, f);
	if (strncmp(buffer, "add", 3) == 0) {
		printf("Operation: addition\n");
		op = OP_ADD;
	}
	else if (strncmp(buffer, "sub", 3) == 0) {
		printf("Operation: subtraction\n");
		op = OP_SUB;
	}
	else if (strncmp(buffer, "hash_generic", 12) == 0) {
		printf("Operation: hash generic\n");
		op = OP_HASH_GENERIC;
	}
	else if (strncmp(buffer, "hash_sha256", 11) == 0) {
		printf("Operation: hash sha256\n");
		op = OP_HASH_SHA256;
	}
	else if (strncmp(buffer, "hash_sha512", 11) == 0) {
		printf("Operation: hash sha512\n");
		op = OP_HASH_SHA512;
	}
	else if (strncmp(buffer, "ec_multiply", 11) == 0) {
		printf("Operation: ec_multiply\n");
		op = OP_EC_MULTIPLY;
	}
	else if (strncmp(buffer, "scalar_multiply", 15) == 0) {
		printf("Operation: scalar multiply\n");
		op = OP_CORE_SCALAR_MULTIPLY;
	}
	else if (strncmp(buffer, "scalar_invert", 13) == 0) {
		printf("Operation: scalar invert\n");
		op = OP_CORE_SCALAR_INVERT;
	}

	if (op == OP_ADD || op == OP_SUB) {
		/* Read 1st number. */
		fgets(buffer, 256, f);
		num1 = atoi(buffer);

		/* Read 2nd number. */
		fgets(buffer, 256, f);
		num2 = atoi(buffer);

		/* Compute result. */
		switch (op) {
		case OP_ADD:
			res = num1 + num2;
			break;
		case OP_SUB:
			res = num1 - num2;
			break;
		default:
			break;
		}
		sprintf(result, "%d", res);
	}
	else if (op == OP_HASH_GENERIC || op == OP_HASH_SHA256 || op == OP_HASH_SHA512) {
		/* Read input message. */
		fgets(buffer, 256, f);
		if (buffer[strlen(buffer)-1] == '\n')
			buffer[strlen(buffer)-1] = '\0';

		/* Compute result. */
		if (op == OP_HASH_GENERIC) {
			unsigned char hash[crypto_generichash_BYTES];
			crypto_generichash(hash, sizeof(hash), (unsigned char *) buffer, strlen(buffer), NULL, 0);
			sodium_bin2hex(result, rlen, hash, sizeof(hash));
		}
		else if (op == OP_HASH_SHA256) {
			unsigned char hash[crypto_hash_sha256_BYTES];
			crypto_hash_sha256(hash, (unsigned char *) buffer, strlen(buffer));
			sodium_bin2hex(result, rlen, hash, sizeof(hash));
		}
		else if (op == OP_HASH_SHA512) {
			unsigned char hash[crypto_hash_sha512_BYTES];
			crypto_hash_sha512(hash, (unsigned char *) buffer, strlen(buffer));
			sodium_bin2hex(result, rlen, hash, sizeof(hash));
		}
	}
	else if (op == OP_EC_MULTIPLY || op == OP_CORE_SCALAR_MULTIPLY || op == OP_CORE_SCALAR_INVERT) {
		/* Read input message. A 32 bytes hex-encoded scalar */
		char input[crypto_core_ed25519_SCALARBYTES*2];
		fgets(input, crypto_core_ed25519_SCALARBYTES*2, f);

		unsigned char s[crypto_core_ed25519_SCALARBYTES];

		sodium_hex2bin(s, crypto_core_ed25519_SCALARBYTES,
				input, crypto_core_ed25519_SCALARBYTES*2,
				NULL, NULL, NULL);

		unsigned char r[crypto_core_ed25519_BYTES];
		if (op == OP_EC_MULTIPLY)
			crypto_scalarmult_ed25519_base_noclamp(r, s);
		else if (op == OP_CORE_SCALAR_MULTIPLY)
			crypto_core_ed25519_scalar_mul(r, s, s);
		else if (op == OP_CORE_SCALAR_INVERT)
			crypto_core_ed25519_scalar_invert(r, s);

		sodium_bin2hex(result, rlen, r, sizeof(r));
	}
	else {
		printf("Unknown operation (%s)", buffer);
		sprintf(result, "Unknown operation (%s)", buffer);
	}

	fclose(f);
}

int main(int argc, char *argv[])
{
	int srv, client;
	int rc;
	unsigned short listen_port = DEFAULT_LISTEN_PORT;
	char addrname[128];
	char command_filename[256], result[256];

	if (argc == 2) {
		printf("argument is %s\n", argv[1]);
		listen_port = atoi(argv[1]);
	}

	/* Initialize libsodium. */
	rc = sodium_init();
	if (rc < 0) {
		printf("Error initializing sodium.\n");
		return -1;
	}

	srv = tcp_create_listener(listen_port, 1);
	if (srv < 0)
		return -1;

	printf("Listening on port %hu...\n", listen_port);
	while (1) {
		ssize_t n;

		client = accept(srv, NULL, 0);
		DIE(client < 0, "accept");

		get_peer_address(client, addrname, 128);
		printf("Received connection from %s\n", addrname);

		/* Read command filename. */
		n = read(client, &command_filename, sizeof(command_filename)-1);
		if (n < 0) {
			printf("Error reading from socket.\n");
			goto close;
		}
		if (n == 0) {
			printf("Connection closed.\n");
			goto close;
		}

		/*
		 * Add NUL terminator to have a valid string.
		 * Remove trailing newline if it's the case.
		 */
		command_filename[n] = '\0';
		if (command_filename[n-1] == '\n')
			command_filename[n-1] = '\0';
		printf("Received: %s\n", command_filename);

		/* Process command filename. */
		process(command_filename, result, 256);

		/* Send output. */
		n = write(client, &result, strlen(result));
		DIE(n < 0, "write");
		printf("Sent: %s\n", result);

close:
		/* Close connection */
		close(client);
	}

	return 0;
}
