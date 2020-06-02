/*@-skipposixheaders@*/
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <dirent.h>
#include <errno.h>
/*@=skipposixheaders@*/
#include "mp-debug.h"
#include "buf_t.h"
#include "mp-jansson.h"
#include "mp-config.h"
#include "mp-dict.h"
#include "mp-memory.h"
#include "mp-ctl.h"

/* Construct config file directory full path */
static buf_t *mp_config_get_config_dir(void)
{
	buf_t         *dirname = buf_new(NULL, 4096);
	int           rc       = -1;
	struct passwd *pw      = NULL;
	const char    *homedir = NULL;

	TESTP(dirname, NULL);

	pw = getpwuid(getuid());
	TESTP(pw, NULL);

	homedir = pw->pw_dir;
	TESTP(homedir, NULL);

	rc = snprintf(dirname->data, dirname->room, "%s/%s", homedir, CONFIG_DIR_NAME);

	/* It must print len + slash */
	if (rc < 0) {
		DE("Wrong file name len\n");
		buf_free(dirname);
		return (NULL);
	}
	dirname->used = rc;
	buf_pack(dirname);
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

	if (0 != stat(filename->data, &statbuf)) {
		DDD("Can't stat config file: %s\n", filename->data);
		goto err;
	}

	if (statbuf.st_size < 1) {
		DE("File size too small\n");
		goto err;
	}

	fd = fopen(filename->data, "r");
	if (NULL == fd) {
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
	return (root);
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

	fd = fopen(filename->data, "w+");
	buf_free(filename);
	if (NULL == fd) {
		rc = -1;
		goto err;
	}

	buf = j_2buf(ctl->config);
	if (NULL == buf || 0 == buf->used) {
		DE("Can't encode config file\n");
		rc = -1;
		goto err;
	}

	written = fwrite(buf->data, 1, buf->used, fd);
	rc = fclose(fd);
	if (0 != rc) {
		DE("Can't close file\n");
		return (EBAD);
	}
	fd = NULL;

	if (written != buf->used) {
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

/* Add new pait 'key' = 'val' into ctl->config and also into config file.
   If such a key exists - replace 'val' */
#if 0 /* SEB 06/05/2020 16:56  */
int mp_config_set(void *_ctl, const char *key, const char *val){
	return EBAD;
}
#endif /* SEB 06/05/2020 16:56 */
