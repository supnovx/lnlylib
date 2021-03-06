#define LNLYLIB_AUTOCONF_CODEGEN
#include "core/prefix.h"

#if defined(L_PLAT_LINUX)
#include "osi/lnxdefs.h"
#elif defined(L_PLAT_WINDOWS)
#include "osi/windefs.h"
#else
#error "unsupported platform"
#endif

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <time.h>
#include <stdlib.h>
#include <stddef.h>
#include <float.h>

#define l_byte unsigned char

static void
l_log_e(const void* fmt, ...)
{
  const l_byte* p = (const l_byte*)fmt;
  va_list args;
  for (; *p; ++p) {
    if (*p != '%') continue;
    if (*(p+1) == '%' || *(p+1) == 's') continue;
    l_log_e("format invalid: %s", fmt);
    return;
  }
  fprintf(stdout, "[E] ");
  va_start(args, fmt);
  vfprintf(stdout, (const char*)fmt, args);
  va_end(args);
  fprintf(stdout, L_NEWLINE);
}

static int
l_write_line(FILE* self, const void* fmt, ...)
{
  va_list args;
  int sz = 0;
  if (!self || !fmt) return 0;
  va_start(args, fmt);
  sz = vfprintf(self, (const char*)fmt, args);
  va_end(args);
  if (sz > 0) fprintf(self, L_NEWLINE);
  else l_log_e("vfprintf %s", strerror(errno));
  return sz;
}

static int
l_write_cdir(FILE* self, const void* fmt)
{
  /** getcwd, get_current_dir_name - get current working directory
  #include <unistd.h>
  char *get_current_dir_name(void);
  it will malloc(3) an array big enough to hold the absolute pathname of the current
  working directory. if the environment variable PWD is set, and its value is correct,
  then that value will be returned. the caller should free(3) the returned buffer. */
#if defined(L_PLAT_WINDOWS)
  #error "[windows] undefined l_write_cdir"
#else
  char* curdir = 0;
  int count = 0;
  curdir = get_current_dir_name();
  if (curdir == 0) {
    l_log_e("get_current_dir_name %s", strerror(errno));
    return 0;
  } else {
    count = l_write_line(self, fmt, curdir);
    free(curdir);
    return count;
  }
#endif
}

typedef union {
  unsigned char a[sizeof(long)];
  unsigned long i;
} l_byteorder;

int main(void)
{
  l_byteorder bytes;
  FILE* file = 0;

  file = fopen("autoconf.h", "wb");
  if (file == 0) {
    l_log_e("fopen autoconf.h %s", strerror(errno));
    return 0;
  }

  l_write_line(file, "#ifndef LNLYLIB_AUTOCONF_H%s#define LNLYLIB_AUTOCONF_H", L_NEWLINE);
  l_write_line(file, "#undef LNLYLIB_AUTOCONF_CODEGEN");
  l_write_line(file, "#define _CRT_SECURE_NO_WARNINGS%s", L_NEWLINE);

  l_write_line(file, "#undef LNLYLIB_HOME_DIR");
  l_write_line(file, "#undef LNLYLIB_CLIB_DIR");
  l_write_line(file, "#undef LNLYLIB_LUALIB_DIR");
  l_write_cdir(file, "#define LNLYLIB_HOME_DIR \"%s\"");
  l_write_line(file, "#define LNLYLIB_LUACLIB_DIR LNLYLIB_HOME_DIR \"/lualib/c\"");
  l_write_line(file, "#define LNLYLIB_LUALIB_DIR LNLYLIB_HOME_DIR \"/lualib\"%s", L_NEWLINE);

  l_write_line(file, "#undef L_MACH_32_BIT");
  l_write_line(file, "#undef L_MACH_64_BIT");
  if (sizeof(void*) == 4) {
    l_write_line(file, "#define L_MACH_32_BIT");
  } else if (sizeof(void*) == 8) {
    l_write_line(file, "#define L_MACH_64_BIT");
  } else {
    l_write_line(file, "#error \"unknown machine of %d-bit\"", sizeof(void*) * 8);
  }

  if (sizeof(long) < 4) {
    l_write_line(file, "#error \"long int size is less than 4-byte\"");
  }
  l_write_line(file, "%s#undef L_LIT_ENDIAN /* lower byte at lower address */", L_NEWLINE);
  l_write_line(file, "#undef L_BIG_ENDIAN /* higher byte at lower address */");
  bytes.i = 0xabcdef;
  if (bytes.a[0] == 0xef) {
    l_write_line(file, "#define L_LIT_ENDIAN");
    if (bytes.a[1] != 0xcd || bytes.a[2] != 0xab || bytes.a[3] != 0x00) {
      l_write_line(file, "#error \"little endian test fail\"");
    }
  } else {
    l_write_line(file, "#define L_BIG_ENDIAN");
    if (bytes.a[0] != 0x00 || bytes.a[1] != 0xab || bytes.a[2] != 0xcd || bytes.a[3] != 0xef) {
      l_write_line(file, "#error \"big endian test fail\"");
    }
  }

  l_write_line(file, "%s#undef false%s#undef true", L_NEWLINE, L_NEWLINE);
  l_write_line(file, "#define false 0%s#define true 1%s", L_NEWLINE, L_NEWLINE);

  l_write_line(file, "#undef l_bool%s#undef l_byte%s#undef l_sbyte", " /* 8-bit integer type */" L_NEWLINE, L_NEWLINE);
  if (sizeof(char) == 1) {
    l_write_line(file, "#define l_bool unsigned char");
    l_write_line(file, "#define l_byte unsigned char");
    l_write_line(file, "#define l_sbyte signed char");
  } else {
    l_write_line(file, "#error \"char size shall be 1-byte\"");
  }

  l_write_line(file, "%s#undef l_short%s#undef l_ushort", L_NEWLINE, " /* 16-bit integer type */" L_NEWLINE);
  if (sizeof(short) == 2) {
    l_write_line(file, "#define l_short short");
    l_write_line(file, "#define l_ushort unsigned short");
  } else {
    l_write_line(file, "#error \"short int size shall be 2-byte\"");
  }

  l_write_line(file, "%s#undef l_medit%s#undef l_umedit", L_NEWLINE, " /* 32-bit integer type */" L_NEWLINE);
  if (sizeof(int) == 4) {
    l_write_line(file, "#define l_medit int");
    l_write_line(file, "#define l_umedit unsigned int");
  } else if (sizeof(long) == 4) {
    l_write_line(file, "#define l_medit long");
    l_write_line(file, "#define l_umedit unsigned long");
  } else {
    l_write_line(file, "#error \"32-bit integer type not found\"");
  }

  l_write_line(file, "%s#undef l_long%s#undef l_ulong", L_NEWLINE, " /* 64-bit integer type */" L_NEWLINE);
  if (sizeof(long) == 8) {
    l_write_line(file, "#define l_long long");
    l_write_line(file, "#define l_ulong unsigned long");
  } else if (sizeof(long long) == 8) {
    l_write_line(file, "#define l_long long long");
    l_write_line(file, "#define l_ulong unsigned long long");
  } else {
    l_write_line(file, "#error \"64-bit integer type not found\"");
  }

  l_write_line(file, "%s#undef l_int%s#undef l_uint", L_NEWLINE, " /* pointer-size integer type */" L_NEWLINE);
  if (sizeof(short) == sizeof(void*)) {
    l_write_line(file, "#define l_int short");
    l_write_line(file, "#define l_uint unsigned short");
  } else if (sizeof(int) == sizeof(void*)) {
    l_write_line(file, "#define l_int int");
    l_write_line(file, "#define l_uint unsigned int");
  } else if (sizeof(long) == sizeof(void*)) {
    l_write_line(file, "#define l_int long");
    l_write_line(file, "#define l_uint unsigned long");
  } else if (sizeof(long long) == sizeof(void*)) {
    l_write_line(file, "#define l_int long long");
    l_write_line(file, "#define l_uint unsigned long long");
  } else {
    l_write_line(file, "#error pointer-size integer type not found");
  }

  l_write_line(file, "%s#undef L_EMPTY_HDL", L_NEWLINE);
  l_write_line(file, "#define L_EMPTY_HDL ((l_filehdl){%s})", L_CODEGEN_FILEHDL_EMPTY_VAL);

  if (L_CODEGEN_FILEHDL_TYPE_SIZE == sizeof(char)) {
    if (L_CODEGEN_FILEHDL_IS_SIGNED) {
      l_write_line(file, "typedef struct { signed char fd; } l_filehdl;");
    } else {
      l_write_line(file, "typedef struct { unsigned char fd; } l_filehdl;");
    }
  } else if (L_CODEGEN_FILEHDL_TYPE_SIZE == sizeof(short)) {
    if (L_CODEGEN_FILEHDL_IS_SIGNED) {
      l_write_line(file, "typedef struct { short fd; } l_filehdl;");
    } else {
      l_write_line(file, "typedef struct { unsigned short fd; } l_filehdl;");
    }
  } else if (L_CODEGEN_FILEHDL_TYPE_SIZE == sizeof(int)) {
    if (L_CODEGEN_FILEHDL_IS_SIGNED) {
      l_write_line(file, "typedef struct { int fd; } l_filehdl;");
    } else {
      l_write_line(file, "typedef struct { unsigned int fd; } l_filehdl;");
    }
  } else if (L_CODEGEN_FILEHDL_TYPE_SIZE == sizeof(long)) {
    if (L_CODEGEN_FILEHDL_IS_SIGNED) {
      l_write_line(file, "typedef struct { long fd; } l_filehdl;");
    } else {
      l_write_line(file, "typedef struct { unsigned long fd; } l_filehdl;");
    }
  } else if (L_CODEGEN_FILEHDL_TYPE_SIZE == sizeof(long long)) {
    if (L_CODEGEN_FILEHDL_IS_SIGNED) {
      l_write_line(file, "typedef struct { long long fd; } l_filehdl;");
    } else {
      l_write_line(file, "typedef struct { unsigned long long fd; } l_filehdl;");
    }
  } else {
    l_write_line(file, "#error \"unsupported filehdl size %d\"", L_CODEGEN_FILEHDL_TYPE_SIZE);
  }

  l_write_line(file, "static l_filehdl l_empty_filehdl() { return L_EMPTY_HDL; }");
  l_write_line(file, "static l_bool l_filehdl_is_empty(l_filehdl* self) { return self->fd == %s; }", L_CODEGEN_FILEHDL_EMPTY_VAL);
  l_write_line(file, "static l_bool l_filehdl_nt_empty(l_filehdl* self) { return self->fd != %s; }", L_CODEGEN_FILEHDL_EMPTY_VAL);

  l_write_line(file, "%s#undef L_IMPL_THRHDL_TYPE_SIZE", L_NEWLINE);
  l_write_line(file, "#undef L_IMPL_THRKEY_TYPE_SIZE");
  l_write_line(file, "#undef L_IMPL_MUTEX_TYPE_SIZE");
  l_write_line(file, "#undef L_IMPL_RWLOCK_TYPE_SIZE");
  l_write_line(file, "#undef L_IMPL_CONDV_TYPE_SIZE");
  l_write_line(file, "#define L_IMPL_THRHDL_TYPE_SIZE %d", L_CODEGEN_THRHDL_TYPE_SIZE);
  l_write_line(file, "#define L_IMPL_THRKEY_TYPE_SIZE %d", L_CODEGEN_THRKEY_TYPE_SIZE);
  l_write_line(file, "#define L_IMPL_MUTEX_TYPE_SIZE %d", L_CODEGEN_MUTEX_TYPE_SIZE);
  l_write_line(file, "#define L_IMPL_RWLOCK_TYPE_SIZE %d", L_CODEGEN_RWLOCK_TYPE_SIZE);
  l_write_line(file, "#define L_IMPL_CONDV_TYPE_SIZE %d", L_CODEGEN_CONDV_TYPE_SIZE);

  l_write_line(file, "%s#undef L_IMPL_SOCKADDR_TYPE_SIZE", L_NEWLINE);
  l_write_line(file, "#undef L_IMPL_IOEVMGR_TYPE_SIZE");
  l_write_line(file, "#define L_IMPL_SOCKADDR_TYPE_SIZE %d", L_CODEGEN_SOCKADDR_TYPE_SIZE);
  l_write_line(file, "#define L_IMPL_IOEVMGR_TYPE_SIZE %d", L_CODEGEN_IOEVMGR_TYPE_SIZE);

  l_write_line(file, "%s#undef L_MAX_FILENAME", L_NEWLINE);
  l_write_line(file, "#define L_MAX_FILENAME %d", FILENAME_MAX);

  l_write_line(file, "%s/* char %d-bit */", L_NEWLINE, sizeof(char)*8);
  l_write_line(file, "/* short %d-bit */", sizeof(short)*8);
  l_write_line(file, "/* int %d-bit */", sizeof(int)*8);
  l_write_line(file, "/* long %d-bit */", sizeof(long)*8);
  l_write_line(file, "/* long long %d-bit */", sizeof(long long)*8);
  l_write_line(file, "/* void* %d-bit */", sizeof(void*)*8);
  l_write_line(file, "/* size_t %d-bit */", sizeof(size_t)*8);
  l_write_line(file, "/* time_t %d-bit */", sizeof(time_t)*8);
  l_write_line(file, "/* clock_t %d-bit */", sizeof(clock_t)*8);
  l_write_line(file, "/* ptrdiff_t %d-bit */", sizeof(ptrdiff_t)*8);
  l_write_line(file, "/* CLOCKS_PER_SEC %d */", CLOCKS_PER_SEC);
  l_write_line(file, "/* BUFSIZ %d */", BUFSIZ);
  l_write_line(file, "/* EOF %d */", EOF);
  l_write_line(file, "/* FILENAME_MAX %d */", FILENAME_MAX);
  l_write_line(file, "/* FOPEN_MAX %d */", FOPEN_MAX);
  l_write_line(file, "/* TMP_MAX %d */", TMP_MAX);
  l_write_line(file, "/* L_tmpnam %d */", L_tmpnam);
  l_write_line(file, "/* RAND_MAX %d */", RAND_MAX);
  l_write_line(file, "/* FLT_EPSILON %.80f */", FLT_EPSILON);
  l_write_line(file, "/* DBL_EPSILON %.80f */", DBL_EPSILON);

  l_write_line(file, "%s#endif /* LNLYLIG_AUTOCONF_H */", L_NEWLINE);
  fclose(file);
  return 0;
}
