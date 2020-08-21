/*@-skipposixheaders@*/
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "openssl/ssl.h"
#include "openssl/err.h"

/*@=skipposixheaders@*/
#include "mp-debug.h"
#include "buf_t/buf_t.h"
#include "mp-jansson.h"
#include "mp-config.h"
#include "mp-dict.h"
#include "mp-memory.h"
#include "mp-ctl.h"
#include "mp-os.h"

/* Construct config file directory full path */
static buf_t *mp_config_get_config_dir(void)
{
	buf_t         *dirname = NULL;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         // buf_string(4096);
	struct passwd *pw      = NULL;
	const char    *homedir = NULL;

	//BUF_DUMP(dirname);

	pw = getpwuid(getuid());
	TESTP(pw, NULL);

	homedir = pw->pw_dir;
	TESTP(homedir, NULL);

	dirname = buf_sprintf("%s/%s", homedir, CONFIG_DIR_NAME);
	return (dirname);
}

/* Construct config file full path */
static buf_t *mp_config_get_config_name(void)
{
	struct passwd *pw      = NULL;
	const char    *homedir = NULL;

	pw = getpwuid(getuid());
	TESTP(pw, NULL);

	homedir = pw->pw_dir;
	TESTP(homedir, NULL);

	return (buf_sprintf("%s/%s/%s", homedir, CONFIG_DIR_NAME, CONFIG_FILE_NAME));
}

/* Construct RSA private key file name */
static buf_t *mp_config_get_rsa_priv_name(void)
{
	struct passwd *pw      = NULL;
	const char    *homedir = NULL;

	pw = getpwuid(getuid());
	TESTP(pw, NULL);

	homedir = pw->pw_dir;
	TESTP(homedir, NULL);

	return (buf_sprintf("%s/%s/%s", homedir, CONFIG_DIR_NAME, RSA_PRIVATE_NAME));
}

/* Construct RSA public key file name */
static buf_t *mp_config_get_rsa_pub_name(void)
{
	struct passwd *pw      = NULL;
	const char    *homedir = NULL;

	pw = getpwuid(getuid());
	TESTP(pw, NULL);

	homedir = pw->pw_dir;
	TESTP(homedir, NULL);

	return (buf_sprintf("%s/%s/%s", homedir, CONFIG_DIR_NAME, RSA_PUBLIC_NAME));
}

/* Construct X509 certificate file name */
static buf_t *mp_config_get_x509_name(void)
{
	struct passwd *pw      = NULL;
	const char    *homedir = NULL;

	pw = getpwuid(getuid());
	TESTP(pw, NULL);

	homedir = pw->pw_dir;
	TESTP(homedir, NULL);

	return (buf_sprintf("%s/%s/%s", homedir, CONFIG_DIR_NAME, X509_CERT_NAME));
}

static err_t mp_config_dir_unlock()
{
	int   rc        = EBAD;
	buf_t *conf_dir = mp_config_get_config_dir();
	TESTP_MES(conf_dir, EBAD, "Can't construct config dir name");
	DDD("conf dir  [%s] room [%d] used [%d]\n", conf_dir->data, conf_dir->room, conf_dir->used);
	/* 1. Change config directory to Read Write mode */
	rc = chmod(conf_dir->data, S_IRUSR | S_IWUSR | S_IXUSR);
	buf_free(conf_dir);
	if (0 != rc) {
		DE("Can't change config dir permition\n");
		return (EBAD);
	}
	return (EOK);
}

static err_t mp_config_dir_lock(void)
{
	int   ret       = EBAD;
	int   rc        = -1;
	buf_t *conf_dir = mp_config_get_config_dir();

	TESTP_MES(conf_dir, EBAD, "Can't construct config dir name");

	/* 1. Change config directory to "Owner Only Read and Exec" mode */
	rc = chmod(conf_dir->data, S_IRUSR | S_IXUSR);
	if (0 != rc) {
		DE("Can't change config dir permition\n");
		goto err;
	}
	ret = EOK;
err:
	buf_free(conf_dir);
	return (ret);
}

/* Change permition of config file and dir to READ / WRITE  */
static err_t mp_config_file_unlock(buf_t *file)
{
	int rc = -1;
	TESTP(file, EBAD);

	rc = mp_config_dir_unlock();
	if (EOK != rc) {
		DE("Can't unlock config dir\n");
		return (EBAD);
	}

	/* 2. Change config file to Read Write mode */
	rc = chmod(file->data, S_IRUSR | S_IWUSR);
	if (0 != rc) {
		DE("Can't change config file [%s] permition\n", file->data);
		goto err;
	}
	return (EOK);
err:
	mp_config_dir_lock();
	return (EBAD);
}

/* Change permition of config file and dir to READ only  */
static err_t mp_config_file_lock(buf_t *file)
{
	int ret = EBAD;
	int rc  = -1;

	TESTP(file, EBAD);

	rc = mp_config_dir_unlock();
	if (EOK != rc) {
		DE("Can't unlock config dir\n");
		goto err;
	}

	/* Change config file to "Owner only Read" mode */
	rc = chmod(file->data, S_IRUSR);
	if (0 != rc) {
		DE("Can't change config file permition\n");
		perror("chmod err:");
		goto err;
	}

	ret = mp_config_dir_lock();
err:
	return (ret);
}

/* Read config file, transform to json obgect and return to caller */
static json_t *mp_config_read(void)
{
	json_t      *root     = NULL;
	FILE        *fd       = NULL;
	buf_t       *filename = NULL;
	struct stat statbuf;
	char        *buf      = NULL;
	size_t      buf_len;
	int         rc        = EBAD;

	filename = mp_config_get_config_name();
	TESTP_MES(filename, NULL, "Can't create config file name");

	rc = mp_config_file_unlock(filename);
	if (EOK != rc) {
		DE("Can't unlock config file\n");
		return (NULL);
	}

	if (0 != stat(filename->data, &statbuf)) {
		DE("Can't stat config file: %s\n", filename->data);
		goto err;
	}

	if (statbuf.st_size < 1) {
		DE("File size too small\n");
		goto err;
	}

	fd = mp_os_fopen(filename->data, "r");
	if (NULL == fd) {
		DE("Can't open config file %s\n", filename->data);
		perror("Open file error:");
		goto err;
	}

	buf_len = (size_t)(statbuf.st_size + 1);

	buf = malloc(buf_len);
	if (NULL == buf) {
		DE("Can't allocate biffer\n");
		goto err;
	}

	memset(buf, 0, buf_len);

	rc = (int)fread(buf, 1, (size_t)statbuf.st_size, fd);
	if (rc != statbuf.st_size) {
		/*@ignore@*/
		DE("can't read config file |%s|: expected %zu, read %d\n", filename->data, statbuf.st_size, rc);
		/*@end@*/
		goto err;
	}

	root = j_str2j(buf);

err:
	buf_free(filename);
	TFREE_SIZE(buf, buf_len);
	if (NULL != fd) {
		if (0 != fclose(fd)) {
			DE("Can't close file\n");
			perror("Can't close file");
		}
	}

	/* Lock config file and dir */
	rc = mp_config_file_lock(filename);
	if (EOK != rc) {
		DE("Can't unlock config file\n");
	}

	return (root);
}

/* Save JSON config object into config file  */
static err_t mp_config_write(j_t *j_config)
{
	int   ret   = EBAD;
	int   rc    = -1;
	int   fd    = -1;
	buf_t *conf = NULL;
	buf_t *file;

	/* Transform from JSON to text form */
	conf = j_buf2j(j_config);
	TESTP_MES(conf, EBAD, "Can't transform config from JSON to buf_t");

	file = mp_config_get_config_name();
	TESTP(file, EBAD);

	rc = mp_config_file_unlock(file);
	if (EOK != rc) {
		DE("Can't unlock config file\n");
		return (EBAD);
	}

	/* Open config file */
	fd = open(file->data, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
	if (fd < 0) {
		DE("Can't open config file\n");
		goto err;
	}

	/* 4. Write config */
	rc = write(fd, conf->data, buf_used(conf));
	if (rc != buf_used(conf)) {
		DE("WARNING: error on config file write: rwritten %d, expected %d\n", rc, buf_used(conf));
	}

	/* Finished. Close and release everything */
	/* Set return status as success */
	ret = EOK;
err:
	if (fd > 0) {
		close(fd);
	}

	rc = mp_config_file_lock(file);
	if (EOK != rc) {
		DE("Can't lock config file / dir\n");
	}

	buf_free(conf);
	buf_free(file);
	return (ret);
}

/* Write config object to config file */
err_t mp_config_save()
{
	FILE   *fd       = NULL;
	buf_t  *filename = NULL;
	buf_t  *dirname  = NULL;
	buf_t  *buf      = NULL;
	int    rc;
	size_t written   = 0;
	/*@shared@*/control_t *ctl = ctl_get();
	DIR    *dir;

	TESTP(ctl, EBAD);
	TESTP(ctl->config, EBAD);

	/* First, test that direcory exists */

	dirname = mp_config_get_config_dir();
	TESTP(dirname, EBAD);

	dir = opendir(dirname->data);
	if (NULL != dir) {
		rc = closedir(dir);
		if (0 != rc) {
			DE("Can't close dir\n");
			perror("can't close dir");
		}
	} else
		if (ENOENT == errno) {
			rc = mkdir(dirname->data, 0700);
			if (0 != rc) {
				DE("mkdir failed; probably it is a;ready exists?\n");
				perror("can't mkdir");
			}
		} else {
			DE("Some error\n");
			perror("Config directory testing error");
			buf_free(dirname);
			return (EBAD);
		}
	buf_free(dirname);

	filename = mp_config_get_config_name();
	TESTP_MES(filename, -1, "Can't create config file name");

	fd = mp_os_fopen(filename->data, "w+");
	if (NULL == fd) {
		DE("Can't open file %s\n", filename->data);
		rc = -1;
		goto err;
	}
	buf_free(filename);

	buf = j_2buf(ctl->config);
	if (NULL == buf || 0 == buf_used(buf)) {
		DE("Can't encode config file\n");
		rc = -1;
		goto err;
	}

	written = fwrite(buf->data, 1, buf_used(buf), fd);
	rc = fclose(fd);
	if (0 != rc) {
		DE("Can't close file\n");
		return (EBAD);
	}
	fd = NULL;

	if (written != (size_t)buf_used(buf)) {
		rc = EBAD;
		goto err;
	}

	rc = EOK;

err:
	if (NULL != buf) {
		if (EOK != buf_free(buf)) {
			DE("Can't remove buf_t: probably passed NULL pointer?\n");
		}
	}
	if (NULL != fd) {
		if (0 != fclose(fd)) {
			DE("Can't close file\n");
			return (EBAD);
		}

	}
	return (rc);
}

/* Create config from content of ctl->me */
err_t mp_config_from_ctl_l()
{
	err_t rc = EBAD;
	/*@shared@*/control_t *ctl = ctl_get();
	if (NULL == ctl->config) {
		ctl->config = j_new();
	}

	TESTP(ctl->config, EBAD);

	ctl_lock();

	rc = j_cp(ctl->me, ctl->config, JK_UID_ME);
	TESTI_MES_GO(rc, err, "Can't copy JK_UID_ME");

	rc = j_cp(ctl->me, ctl->config, JK_NAME);
	TESTI_MES_GO(rc, err, "Can't copy JK_NAME");

	rc = j_add_str(ctl->config, JK_SOURCE, JV_YES);
	TESTI_MES_GO(rc, err, "Can't add JK_SOURCE");

	rc = j_add_str(ctl->config, JK_TARGET, JV_YES);
	TESTI_MES_GO(rc, err, "Can't add JK_TARGET");

	rc = j_add_str(ctl->config, JK_BRIDGE, JV_YES);
	TESTI_MES_GO(rc, err, "Can't add JK_BRIDGE");
	ctl_unlock();

	return (mp_config_save());
err:
	ctl_unlock();
	return (EBAD);
}

/* Read config from file. If there is no config file - return error */
err_t mp_config_load()
{
	err_t      rc   = EBAD;
	const char *key = NULL;
	json_t     *val = NULL;
	/*@shared@*/control_t *ctl = ctl_get();
	ctl->config = mp_config_read();
	if (NULL == ctl->config) {
		return (EBAD);
	}

	/* Loaded. Let's init ctl->me with the fields from the control */
	json_object_foreach(ctl->config, key, val) {
		DDD("Going to copy %s from ctl->config to ctl->me\n", key);
		rc = j_cp(ctl->config, ctl->me, key);
		TESTI_MES(rc, EBAD, "Can't copy object from ctl->config to ctl->me");
	}

	return (rc);
}

/* Add new pair 'key' = 'val' into ctl->config and also into config file.
   If such a key exists - replace 'val' */
err_t mp_config_set(const char *key, const char *val)
{
	control_t *ctl    = ctl_get();
	j_t       *j_conf = ctl->config;

	if (NULL == j_conf) {
		DE("Looks like config is empty\n");
		return (EBAD);
	}

	j_add_str(j_conf, key, val);

	/* Now save config to the file */

	return (mp_config_write(j_conf));
}


// https://stackoverflow.com/questions/5367991/c-openssl-export-private-key
// https://www.openssl.org/docs/man1.1.1/man3/PEM_write_bio_RSAPrivateKey.html
// https://stackoverflow.com/questions/5927164/how-to-generate-rsa-private-key-using-openssl

int mp_config_save_rsa_keys(RSA *rsa)
{
	err_t      ret   = EBAD;
	int        rc    = -1;
	EVP_CIPHER *enc  = NULL;
	buf_t      *file = NULL;
	FILE       *fp   = NULL;

	/* Part 1: write private key */
	file = mp_config_get_rsa_priv_name();
	TESTP(file, EBAD);
	rc = mp_config_file_unlock(file);
	TESTI_GO(rc, err);


	fp = fopen(file->data, "wb");
	TESTP(fp, EBAD);

	/* Todo: password the key */
	rc = PEM_write_RSAPrivateKey(fp, rsa, enc, NULL, 0, NULL, NULL);

	fflush(fp);
	fclose(fp);

	if (0 == rc) {
		DE("Can't write private key\n");
		goto err;
	}

	rc = mp_config_file_lock(file);
	TESTI_GO(rc, err);
	buf_free(file);

	/* Part 2: save public key */
	file = mp_config_get_rsa_pub_name();
	TESTP(file, EBAD);

	rc = mp_config_file_unlock(file);
	TESTI_GO(rc, err);

	fp = fopen(file->data, "wb");
	TESTP(fp, EBAD);

	rc = PEM_write_RSAPublicKey(fp, rsa);

	fflush(fp);
	fclose(fp);

	if (0 == rc) {
		DE("Can't write public key");
		goto err;
	}

	rc = mp_config_file_lock(file);
	TESTI_GO(rc, err);

	ret = EOK;

err:
	buf_free(file);
	return (ret);
}

err_t mp_config_save_rsa_x509(X509 *x509)
{
	err_t ret   = EBAD;
	int   rc    = EBAD;
	FILE  *fd   = NULL;
	buf_t *file = NULL;

	file = mp_config_get_x509_name();
	TESTP(file, EBAD);

	rc = mp_config_file_unlock(file);
	TESTI_GO(rc, err);

	/* Open file for writing */
	fd = fopen(file->data, "wb");
	if (NULL == fd) {
		DE("Can't open x509 file for writing\n");
		goto err;
	}

	/* Write the X509 to the file */
	rc = PEM_write_X509(fd, x509);
	if (0 == rc) {
		DE("Can't write X509 certificate: error in PEM_write_X509\n");
		abort();
	}

	fflush(fd);
	ret = EOK;

err:
	rc = mp_config_file_lock(file);
	if (EOK != rc) {
		DE("WARNING: Can't lock config file\n");
	}

	buf_free(file);
	return (ret);
}

X509 *mp_config_load_X509()
{
	int   rc    = EBAD;
	buf_t *file = NULL;
	X509  *x509 = NULL;
	FILE  *fp   = NULL;

	file = mp_config_get_x509_name();
	TESTP_GO(file, err);

	rc = mp_config_file_unlock(file);
	TESTI_GO(rc, err);

	fp = fopen(file->data, "r");
	TESTP_GO(fp, err);

	x509 = PEM_read_X509(fp, NULL, NULL, NULL);
	fclose(fp);

err:
	rc = mp_config_file_lock(file);
	if (EOK != rc) {
		DE("Can't lock config file\n");
	}
	buf_free(file);
	return (x509);
}

/* Load public key from pem file */
RSA *mp_config_load_rsa_pub()
{
	int   rc    = EBAD;
	RSA   *rsa  = NULL;
	buf_t *file = NULL;
	FILE  *fp   = NULL;

	file = mp_config_get_rsa_pub_name();
	TESTP_GO(file, err);

	rc = mp_config_file_unlock(file);
	TESTI_GO(rc, err);

	fp = fopen(file->data, "rb");
	TESTP_GO(fp, err);

	rsa = PEM_read_RSAPublicKey(fp, NULL, NULL, NULL);
	fclose(fp);
	fp = NULL;

	if (NULL == rsa) {
		DE("Can't read RSA public key from %s\n", file->data);
		ERR_print_errors_fp(stderr);
		abort();
	}

err:
	rc = mp_config_file_lock(file);
	buf_free(file);
	return (rsa);
}

/* Probe if RSA private file exists */
err_t mp_config_probe_rsa_priv()
{
	int         rc;
	buf_t       *file = NULL;

	struct stat st;

	rc = mp_config_dir_unlock();
	if (EOK != rc) {
		DE("Can't unlock config dir\n");
		return (EBAD);
	}

	file = mp_config_get_rsa_priv_name();
	TESTP(file, EBAD);

	stat(file->data, &st);
	buf_free(file);

	rc = mp_config_dir_lock();
	if (EOK != rc) {
		DE("Can't lock config dir\n");
	}

	if (S_IFREG == (st.st_mode & S_IFMT)) {
		return (EOK);
	}

	return (EBAD);
}

/* Probe if RSA private file exists */
err_t mp_config_probe_x509()
{
	int         rc;
	buf_t       *file = NULL;

	struct stat st;

	rc = mp_config_dir_unlock();
	if (EOK != rc) {
		DE("Can't unlock config dir\n");
		return (EBAD);
	}

	file = mp_config_get_x509_name();
	TESTP(file, EBAD);

	stat(file->data, &st);
	buf_free(file);

	rc = mp_config_dir_lock();
	if (EOK != rc) {
		DE("Can't lock config dir\n");
	}

	if (S_IFREG == (st.st_mode & S_IFMT)) {
		return (EOK);
	}
	return (EBAD);
}

/* Load public key from pem file */
RSA *mp_config_load_rsa_priv()
{
	int   rc    = EBAD;
	RSA   *rsa  = NULL;
	buf_t *file = NULL;
	FILE  *fp   = NULL;

	file = mp_config_get_rsa_priv_name();
	TESTP_GO(file, err);

	rc = mp_config_file_unlock(file);
	TESTI_GO(rc, err);

	fp = fopen(file->data, "rb");
	if (NULL == fp) {
		DE("Can't open file %s\n", file->data);
		perror("Can't open:");
		goto err;
	}

	rsa = PEM_read_RSAPrivateKey(fp, NULL, NULL, NULL);
	fclose(fp);
	fp = NULL;

	if (NULL == rsa) {
		DE("Can't read RSA private key from %s\n", file->data);
		ERR_print_errors_fp(stderr);
		abort();
	}

err:
	rc = mp_config_file_lock(file);
	buf_free(file);
	return (rsa);
}