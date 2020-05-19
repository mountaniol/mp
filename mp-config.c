/*@-skipposixheaders@*/
#include <string.h>
#include <sys/stat.h>
/*@=skipposixheaders@*/
#include "mp-common.h"
#include "mp-debug.h"
#include "buf_t.h"
#include "mp-jansson.h"
#include "mp-config.h"
#include "mp-dict.h"
#include "mp-memory.h"
#include "mp-common.h"
#include "mp-ctl.h"
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <dirent.h>
#include <errno.h>

/* Construct config file directory full path */
static char *mp_config_get_config_dir(void)
{
	char *dirname = NULL;
	int rc = -1;
	size_t len = 0;
	struct passwd *pw = NULL;
	const char *homedir = NULL;

	pw = getpwuid(getuid());
	TESTP(pw, NULL);

	homedir = pw->pw_dir;
	TESTP(homedir, NULL);

	len = strlen(homedir) + strlen(CONFIG_DIR_NAME);

	/* Filename len: directory + slash + file config dir + '\0' */
	dirname = zmalloc(len + 2);

	TESTP_MES(dirname, NULL, "Can't allocate filepath");

	rc = snprintf(dirname, len + 2, "%s/%s", homedir, CONFIG_DIR_NAME);

	/* It must print len + slash */
	if (-1 == rc || (size_t)rc != (len + 1)) {
		DE("Wrong file name len\n");
	}
	return (dirname);
}

/* Construct config file full path */
static char *mp_config_get_config_name(void)
{
	char *filename = NULL;
	int rc = -1;
	size_t len = 0;
	struct passwd *pw = NULL;
	const char *homedir = NULL;

	pw = getpwuid(getuid());
	TESTP(pw, NULL);

	homedir = pw->pw_dir;
	TESTP(homedir, NULL);

	len = strlen(homedir) + strlen(CONFIG_DIR_NAME) + strlen(CONFIG_FILE_NAME);

	/* Filename len: directory + slash + file config dir + slash + file name + '\0' */
	filename = zmalloc(len + 3);

	TESTP_MES(filename, NULL, "Can't allocate filepath");

	rc = snprintf(filename, len + 3, "%s/%s/%s", homedir, CONFIG_DIR_NAME, CONFIG_FILE_NAME);

	/* It must print len + slash */
	if (-1 == rc || (size_t)rc != (len + 2)) {
		/*@ignore@*/
		DE("Wrong file name len : rc = %d, expected %zu\n", rc, len + 2);
		/*@end@*/
	}
	return (filename);
}

/* Read config file, transform to json obgect and return to caller */
static json_t *mp_config_read(void)
{
	json_t *root = NULL;
	FILE *fd = NULL;
	char *filename = NULL;
	struct stat statbuf;
	char *buf = NULL;
	int rc = EBAD;

	filename = mp_config_get_config_name();
	TESTP_MES(filename, NULL, "Can't create config file name");

	if (0 != stat(filename, &statbuf)) {
		DDD("Can't stat config file: %s\n", filename);
		goto err;
	}

	if (statbuf.st_size < 1) {
		DE("File size too small\n");
		goto err;
	}

	fd = fopen(filename, "r");
	if (NULL == fd) {
		goto err;
	}

	buf = malloc((size_t)(statbuf.st_size + 1));
	if (NULL == buf) {
		DE("Can't allocate biffer\n");
		goto err;
	}

	memset(buf, 0, (size_t)statbuf.st_size + 1);


	rc = (int)fread(buf, 1, (size_t)statbuf.st_size, fd);
	if (rc != statbuf.st_size) {
		/*@ignore@*/
		DE("can't read config file: expected %zu, read %d\n", statbuf.st_size, rc);
		/*@end@*/
		goto err;
	}

	root = j_str2j(buf);

err:
	TFREE(filename);
	TFREE(buf);
	if (fd) {
		if (0 != fclose(fd)) {
			DE("Can't close file\n");
			perror("Can't close file");
		}
	}
	return (root);

}

/* Write config object to config file */
err_t mp_config_save()
{
	FILE *fd = NULL;
	char *filename = NULL;
	char *dirname = NULL;
	buf_t *buf = NULL;
	int rc;
	size_t written = 0;
	/*@shared@*/control_t *ctl = ctl_get();
	DIR *dir;

	TESTP(ctl, EBAD);
	TESTP(ctl->config, EBAD);

	/* First, test that direcory exists */

	dirname = mp_config_get_config_dir();
	TESTP(dirname, EBAD);

	dir = opendir(dirname);
	if (NULL != dir) {
		rc = closedir(dir);
		if (0 != rc) {
			DE("Can't close dir\n");
			perror("can't close dir");
		}
	} else if (ENOENT == errno) {
		rc = mkdir(dirname, 0700);
		DE("mkdir failed; probably it is a;ready exists?\n");
		perror("can't mkdir");
	} else {
		DE("Some error\n");
		perror("Config directory testing error");
		TFREE(dirname);
		return (EBAD);
	}
	TFREE(dirname);

	filename = mp_config_get_config_name();
	TESTP_MES(filename, -1, "Can't create config file name");

	fd = fopen(filename, "w+");
	if (NULL == fd) {
		rc = -1;
		goto err;
	}

	buf = j_2buf(ctl->config);
	if (NULL == buf || 0 == buf->len) {
		DE("Can't encode config file\n");
		rc = -1;
		goto err;
	}

	written = fwrite(buf->data, 1, buf->len, fd);
	rc = fclose(fd);
	if (0 != rc) {
		DE("Can't close file\n");
		return (EBAD);
	}
	fd = NULL;

	if (written != buf->len) {
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
	err_t rc = EBAD;
	const char *key = NULL;
	json_t *val = NULL;
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

/* Add new pait 'key' = 'val' into ctl->config and also into config file.
   If such a key exists - replace 'val' */
#if 0 /* SEB 06/05/2020 16:56  */
int mp_config_set(void *_ctl, const char *key, const char *val){
	return EBAD;
}
#endif /* SEB 06/05/2020 16:56 */
