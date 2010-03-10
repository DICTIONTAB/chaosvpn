#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "list.h"
#include "string/string.h"
#include "main.h"
#include "config.h"
#include "fs.h"
#include "tinc.h"


extern FILE *yyin;
extern int yyparse(void);

struct config *globalconfig = NULL;


struct config*
config_alloc(void)
{
	struct config *config;

	config = malloc(sizeof(struct config));
	if (config == NULL) return NULL;
	memset(config, 0, sizeof(struct config));

	config->peerid			= NULL;
	config->vpn_ip			= NULL;
	config->vpn_ip6			= NULL;
	config->networkname		= NULL;
	config->my_ip			= NULL;
	config->tincd_bin		= strdup("/usr/sbin/tincd");
	config->tincd_debuglevel	= 3;
	config->tincd_restart_delay	= 20;
	config->routemetric		= strdup("0");
	config->routeadd		= NULL;
	config->routeadd6		= NULL;
	config->routedel		= NULL;
	config->routedel6		= NULL;
	config->ifconfig		= NULL;
	config->ifconfig6		= NULL; // not required
	config->master_url		= strdup("https://www.vpn.hamburg.ccc.de/tinc-chaosvpn.txt");
	config->base_path		= strdup("/etc/tinc");
	config->pidfile			= strdup("/var/run/chaosvpn.default.pid");
	config->my_peer			= NULL;
	config->masterdata_signkey	= NULL;
	config->tincd_graphdumpfile	= NULL;
	config->update_interval		= 0;
	config->ifmodifiedsince = 0;

	string_lazyinit(&config->privkey, 2048);
	INIT_LIST_HEAD(&config->peer_config);

	config->configfile		= strdup("/etc/tinc/chaosvpn.conf");
	config->donotfork		= false;

	return config;
}

int
config_init(struct config *config)
{
	struct stat st; 
	struct string privkey_name;

	globalconfig = config;

	yyin = fopen(config->configfile, "r");
	if (!yyin) {
		(void)fprintf(stderr, "Error: unable to open %s\n", config->configfile);
		return 1;
	}
	yyparse();
	fclose(yyin);

	if ((config->update_interval == 0) && (!config->donotfork)) {
		(void)fputs("Error: you have not configured a remote config update interval.\n" \
				"($update_interval) Please configure an interval (3600 - 7200 seconds\n" \
				"are recommended) or activate legacy (cron) mode by using the -a flag.\n", stderr);
		exit(1);
	}
	if ((config->update_interval < 60) && (!config->donotfork)) {
		(void)fputs("Error: $update_interval may not be <60.\n", stderr);
		exit(1);
	}


	// check required params
	#define reqparam(paramfield, label) if (str_is_empty(config->paramfield)) { \
		fprintf(stderr, "%s is missing or empty in %s\n", label, config->configfile); \
		return 1; \
		}

	reqparam(peerid, "$my_peerid");
	reqparam(networkname, "$networkname");
	reqparam(vpn_ip, "$my_vpn_ip");
	reqparam(routeadd, "$routeadd");
	reqparam(ifconfig, "$ifconfig");
	reqparam(base_path, "$base");


	// create base directory
	if (stat(config->base_path, &st) & fs_mkdir_p(config->base_path, 0700)) {
		fprintf(stderr, "error: unable to mkdir %s\n", config->base_path);
		return 1;
	}

	string_init(&privkey_name, 1024, 512);
	if (string_concat_sprintf(&privkey_name, "%s/rsa_key.priv", config->base_path)) { return 1; }

	string_free(&config->privkey); /* just to be sure */
	if (fs_read_file(&config->privkey, string_get(&privkey_name))) {
		fprintf(stderr, "error: can't read private rsa key at %s\n", string_get(&privkey_name));
		string_free(&privkey_name);
		return 1;
	}
	string_free(&privkey_name);


	config->tincd_version = tinc_get_version(config);
	if (config->tincd_version == NULL) {
		fprintf(stderr, "Warning: cant determine tinc version!\n");
	}

	return 0;
}

/* get pointer to already allocated and initialized config structure */
struct config*
config_get(void) {
	return globalconfig;
}