
#ifndef __NAF_H__
#define __NAF_H__

int naf_init0(const char *appname, const char *appver, const char *appdesc, const char *appcopyright);
int naf_init1(int argc, char **argv);
int naf_init_final(void);
int naf_main(void);

struct naf_appinfo {
	char *nai_name;
	char *nai_version;
	char *nai_description;
	char *nai_copyright;
};
extern struct naf_appinfo naf_curappinfo;

#endif /* ndef __NAF_H__ */

