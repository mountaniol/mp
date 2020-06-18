/* 
 * This file implements security related functionality:
 * Secure connection
 * Private / public keys generation
 * Certificate generation
 * Sectificate validation
 * Certificate signment
 * USB HSM access
 */

#include <sys/types.h>
#include <pwd.h>
#include <stdio.h>
#include <unistd.h>

#include "openssl/ssl.h"
#include "openssl/err.h"

#include "buf_t.h"
#include "mp-debug.h"
#include "mp-common.h"
#include "mp-security.h"

/* OpenSSL connection functions */

/* Create SSL context */
/*@null@*/ SSL_CTX *mp_security_init_server_tls_ctx(void)
{
	const SSL_METHOD *method;
	SSL_CTX          *ctx;

	/* TODO: Do we really need all algorythms? */
	OpenSSL_add_all_algorithms();      /* load & register all cryptos, etc. */
	SSL_load_error_strings();       /* load all error messages */
	method = TLS_server_method();
	ctx = SSL_CTX_new(method);       /* create new context from method */
	if (ctx == NULL) {
		ERR_print_errors_fp(stderr);
		return (NULL);
	}

	/* Establish connection automatically; enable partial write */
	(void)SSL_CTX_set_mode(ctx, (long int)(SSL_MODE_AUTO_RETRY | SSL_MODE_ENABLE_PARTIAL_WRITE));
	return (ctx);
}


/* Load certificate + private key from files */
/* TODO: we may need also function loading all this from memory (SSL_FILETYPE_ASN1) */
err_t mp_security_load_certs(SSL_CTX *ctx, char *cert_file, char *priv_key)
{
	/* set the local certificate from CertFile */
	if (SSL_CTX_use_certificate_file(ctx, cert_file, SSL_FILETYPE_PEM) <= 0) {
		ERR_print_errors_fp(stderr);
		return (EBAD);
	}
	/* set the private key from KeyFile (may be the same as CertFile) */
	if (SSL_CTX_use_PrivateKey_file(ctx, priv_key, SSL_FILETYPE_PEM) <= 0) {
		ERR_print_errors_fp(stderr);
		return (EBAD);
	}
	/* verify private key */
	if (!SSL_CTX_check_private_key(ctx)) {
		DE("Private key does not match the public certificate\n");
		return (EBAD);
	}

	return (EOK);
}

/* Generate RSA private key, return in in RSA  format */
/* The password applied later, when we write it down to the disk */
/* Input param - size of the key (2048 is the best) */
RSA *mp_security_generate_rsa_pem_RSA(const int kbits)
{
	int      rc;
	BN_ULONG exp  = 17;

	BIGNUM   *bne = NULL;
	RSA      *rsa = NULL;

	bne = BN_new();
	TESTP(bne, NULL);

	rc = BN_set_word(bne, exp);
	if (1 != rc) {
		DE("Can't set BUG NUM\n");
		return (NULL);
	}

	rsa = RSA_new();
	TESTP(rsa, NULL);
	rc = RSA_generate_key_ex(rsa, kbits, bne, NULL);
	BN_free(bne);

	if (1 != rc) {
		DE("Can't generate RSA key\n");
	}

	return (rsa);
}

/* Generate RSA private key, return in in PEM format */
/* The password applied later, when we write it down to the disk */
buf_t *mp_security_generate_rsa_pem_string(const int kbits)
{
	int   keylen;

	RSA   *rsa   = NULL;
	buf_t *buf   = NULL;

	rsa = mp_security_generate_rsa_pem_RSA(kbits);
	TESTP(rsa, NULL);

	/* To get the C-string PEM form: */
	BIO *bio = BIO_new(BIO_s_mem());
	PEM_write_bio_RSAPrivateKey(bio, rsa, NULL, NULL, 0, NULL, NULL);

	keylen = BIO_pending(bio);
	buf = buf_string(keylen + 1);
	TESTP(buf, NULL);
	BUF_DUMP(buf);
	//pem_key = calloc(keylen + 1, 1); /* Null-terminate */
	BIO_read(bio, buf->data, keylen);
	buf->used = keylen + 1;

	//printf("%s", pem_key);
	printf("%s", buf->data);

	BIO_free_all(bio);
	RSA_free(rsa);
	//free(pem_key);
	return (buf);
}

/*@null@*/ buf_t *mp_security_sha256_string(/*@null@*/buf_t *buf)
{
	int           i    = 0;
	buf_t         *ret = NULL;
	unsigned char hash[SHA256_DIGEST_LENGTH];
	TESTP(buf, NULL);

	SHA256_CTX sha256;
	SHA256_Init(&sha256);
	SHA256_Update(&sha256, buf->data, buf_used(buf));
	SHA256_Final(hash, &sha256);

	ret = buf_string(64+1);
	TESTP(ret, NULL);

	BUF_DUMP(ret);

	for (i = 0; i < SHA256_DIGEST_LENGTH; i++) {
		sprintf(ret->data + (i * 2), "%02x", hash[i] );
	}
	ret->data[64] = 0;
	ret->used = 64;
	return (ret);
}

/*@null@*/buf_t *mp_security_system_footprint()
{
	int           rc        = -1;
	buf_t         *buf      = NULL;
	buf_t         *ret      = NULL;

	/* Name of this host */
	buf_t         *buf_host = NULL;
	struct passwd *pw       = NULL;
	/* Home directory of current user */
	const char    *homedir  = NULL;

	/* Number of processors in this system */
	int           procs     = sysconf(_SC_NPROCESSORS_CONF);

	pw = getpwuid(getuid());
	TESTP(pw, NULL);

	homedir = pw->pw_dir;
	buf_host = buf_string(256);
	TESTP(buf_host, NULL);

	BUF_DUMP(buf_host);
	BUF_TEST(buf_host); 
	rc = gethostname(buf_host->data, buf_room(buf_host - 4));

	if (rc < 0) {
		goto err;
	}
	buf = buf_sprintf("%s-%s-%d", buf_host->data, homedir, procs);
	buf_free(buf_host);

	if (NULL == buf) {
		goto err;
	}

	DDD("RSA  string: |%s|\n", buf->data);

	ret = mp_security_sha256_string(buf);
	buf_free(buf);
	DDD("Hash string: |%s|\n", ret->data);

	return (ret);
err:
	buf_free(buf_host);
	buf_free(buf);
	return (NULL);
}