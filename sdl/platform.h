#pragma once
/* The few operating-system calls the frontend needs, for POSIX and Windows (mingw-w64). */
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#include <process.h>
#include <windows.h>
static inline int plat_mkdir(const char *p) { return _mkdir(p); }
static inline int plat_rmdir(const char *p) { return _rmdir(p); }
static inline void plat_setenv(const char *k, const char *v) { _putenv_s(k, v ? v : ""); }   /* ("" unsets) */
/* a new empty directory in the temporary folder, named after prefix; 0 on failure */
static inline int plat_temp_dir(char *out, size_t n, const char *prefix)
{
	char base[MAX_PATH]; DWORD k = GetTempPathA(sizeof base, base); if (!k || k >= sizeof base) return 0;
	for (unsigned i = 0; i < 100; i++) {
		snprintf(out, n, "%s%s-%lu-%lu-%u", base, prefix, (unsigned long)GetCurrentProcessId(), (unsigned long)GetTickCount(), i);
		if (_mkdir(out) == 0) return 1;
	}
	return 0;
}
#else
#include <sys/stat.h>
#include <unistd.h>
static inline int plat_mkdir(const char *p) { return mkdir(p, 0777); }
static inline int plat_rmdir(const char *p) { return rmdir(p); }
static inline void plat_setenv(const char *k, const char *v) { if (v) setenv(k, v, 1); else unsetenv(k); }
static inline int plat_temp_dir(char *out, size_t n, const char *prefix)
{
	const char *t = getenv("TMPDIR"); snprintf(out, n, "%s/%s-XXXXXX", t && *t ? t : "/tmp", prefix);
	return mkdtemp(out) != NULL;
}
#endif
/* a regular file exists at p */
static inline int plat_file_exists(const char *p)
{
	struct stat st; if (!p || stat(p, &st)) return 0;
#ifdef _WIN32
	return (st.st_mode & _S_IFMT) == _S_IFREG;
#else
	return S_ISREG(st.st_mode);
#endif
}
