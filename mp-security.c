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

#include "buf_t/buf_t.h"
#include "mp-debug.h"
#include "mp-common.h"
#include "mp-security.h"
#include "mp-ctl.h"

/* OpenSSL connection functions */

/* Don't load certificates from the file, use in-memory */
err_t mp_security_use_certs(SSL_CTX *ctx, void *x509, void *priv_rsa)
{
	/* set the local certificate from CertFile */
	if (SSL_CTX_use_certificate(ctx, x509) <= 0) {
		DE("Can't use ctl->x509 certificate\n");
		ERR_print_errors_fp(stderr);
		return (EBAD);
	}

	/* set the private key from KeyFile (may be the same as CertFile) */
	if (SSL_CTX_use_RSAPrivateKey(ctx, priv_rsa) <= 0) {
		DE("Can't use ctl->rsa private key\n");
		ERR_print_errors_fp(stderr);
		return (EBAD);
	}

	/* verify private key */
	if (!SSL_CTX_check_private_key(ctx)) {
		DE("Private key does not match the public certificate\n");
		return (EBAD);
	}

	DD("Success: created OpenSSL CTX object\n");
	return (EOK);
}

/* Create SSL context (CTX) */
/*@null@*/ SSL_CTX *mp_security_init_server_tls_ctx(void)
{
	control_t        *ctl    = ctl_get();
	const SSL_METHOD *method;
	SSL_CTX          *ctx;

	/* TODO: Do we really need all algorythms? */
	/* No return value */
	OpenSSL_add_all_algorithms();      /* load & register all cryptos, etc. */

	/* No return value */
	SSL_load_error_strings();       /* load all error messages */
	method = TLS_server_method();
	if (NULL == method) {
		DE("Can't create 'method' for CTX context");
		return (NULL);
	}

	ctx = SSL_CTX_new(method);       /* create new context from method */
	if (ctx == NULL) {
		DE("Can't create OpenSSL 'ctx' object\n");
		ERR_print_errors_fp(stderr);
		return (NULL);
	}

	/* Establish connection automatically; enable partial write */
	/* THe return value of this function is useless for us */
	(void)SSL_CTX_set_mode(ctx, (long int)(SSL_MODE_AUTO_RETRY | SSL_MODE_ENABLE_PARTIAL_WRITE));

	/* Load certificates */
	/* Before the CTX set to use X509 cert and RSA private key, they should be created and loaded  */
	if (NULL == ctl->x509) {
		DE("Can not proceed - no X509 certificate loaded\n");

	}
	//if (EOK != mp_security_load_certs(ctx, ctl->x509, ctl->rsa_priv)) {
	if (EOK != mp_security_use_certs(ctx, ctl->x509, ctl->rsa_priv)) {
		SSL_CTX_free(ctx);
		return (NULL);
	}

	return (ctx);
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

	ret = buf_string(64 + 1);
	TESTP(ret, NULL);

	//BUF_DUMP(ret);

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

	//BUF_DUMP(buf_host);
	BUF_TEST(buf_host);
	rc = gethostname(buf_host->data, buf_room(buf_host - 4));
	buf_detect_used(buf_host);

	if (rc < 0) {
		goto err;
	}
	buf = buf_sprintf("%s-%s-%d", buf_host->data, homedir, procs);
	//BUF_DUMP(buf);
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


// https://stackoverflow.com/questions/256405/programmatically-create-x509-certificate-using-openssl
// http://www.opensource.apple.com/source/OpenSSL/OpenSSL-22/openssl/demos/x509/mkcert.c
/*@null@*/X509 *mp_security_generate_x509(RSA *rsa)
{
	EVP_PKEY   *pkey = NULL;
	X509       *x509 = NULL;
	X509_NAME  *name = NULL;

	const char *user = NULL;
	const char *uid  = NULL;

	pkey = EVP_PKEY_new();
	EVP_PKEY_assign_RSA(pkey, rsa);

	x509 = X509_new();
	ASN1_INTEGER_set(X509_get_serialNumber(x509), 1);

	/* Set certificate generation time */
	X509_gmtime_adj(X509_get_notBefore(x509), 0);

	/* Set experation time after 100 years */
	X509_gmtime_adj(X509_get_notAfter(x509), 3153600000L);

	/* Set public key to this certificate */
	X509_set_pubkey(x509, pkey);

	name = X509_get_subject_name(x509);

	/* Set certificate properties */

	/* TODO: Set country; for now it sets UK hardcoded */
	X509_NAME_add_entry_by_txt(name, "C", MBSTRING_ASC, (unsigned char *)"UK", -1, -1, 0);

	/* Set company: We use user name as company name */
	user = ctl_user_get();
	X509_NAME_add_entry_by_txt(name, "O", MBSTRING_ASC, (unsigned char *)user, -1, -1, 0);
	/* Set host: we use UID as the host name */
	uid = ctl_uid_get();
	X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (unsigned char *)uid, -1, -1, 0);

	X509_set_issuer_name(x509, name);

	/* Sign certificate */
	X509_sign(x509, pkey, EVP_sha1());

	/* Clean everything */

	return (x509);
}