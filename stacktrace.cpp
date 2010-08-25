/*
  A hacky replacement for backtrace_symbols in glibc

  backtrace_symbols in glibc looks up symbols using dladdr which is limited in
  the symbols that it sees. libbacktracesymbols opens the executable and shared
  libraries using libbfd and will look up backtrace information using the symbol
  table and the dwarf line information.

  It may make more sense for this program to use libelf instead of libbfd.
  However, I have not investigated that yet.

  Derived from addr2line.c from GNU Binutils by Jeff Muizelaar

  Copyright 2007 Jeff Muizelaar
*/

/* addr2line.c -- convert addresses to line number and function name
   Copyright 1997, 1998, 1999, 2000, 2001, 2002 Free Software Foundation, Inc.
   Contributed by Ulrich Lauther <Ulrich.Lauther@mchp.siemens.de>

   This file was part of GNU Binutils.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#define fatal(a) exit(1)
#define bfd_fatal(a) exit(1)
#define bfd_nonfatal(a) exit(1)
#define list_matching_formats(a) exit(1)

/* 2 characters for each byte, plus 1 each for 0, x, and NULL */
#define PTRSTR_LEN (sizeof(void *) * 2 + 3)
#define true 1
#define false 0

#ifndef __cplusplus
#define _GNU_SOURCE
#endif
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <execinfo.h>
#include <bfd.h>
#define HAVE_DECL_BASENAME 1
#include <libiberty.h>
#include <dlfcn.h>
#include <link.h>

#ifdef __cplusplus
#include <cxxabi.h>
#endif
#include <signal.h>


static asymbol **syms;		/* Symbol table.  */

/* 150 isn't special; it's just an arbitrary non-ASCII char value.  */
#define OPTION_DEMANGLER	(150)

static void slurp_symtab(bfd * abfd);
static void find_address_in_section(bfd *abfd, asection *section, void *data);
char* read_line_from_file(const char *filename, unsigned int line_number);
static char **process_file(const char *file_name, bfd_vma *addr, int naddr);

/* These global variables are used to pass information between
   translate_addresses and find_address_in_section.  */

static bfd_vma pc;
static const char *filename;
static const char *functionname;
static unsigned int line;
static int found;


static char** translate_addresses_buf(bfd * abfd, bfd_vma *addr, int naddr)
{
	int naddr_orig = naddr;
	char b;
	int total  = 0;
	enum { Count, Print } state;
	char *buf = &b;
	int len = 0;
	char **ret_buf = NULL;
	/* iterate over the formating twice.
	 * the first time we count how much space we need
	 * the second time we do the actual printing */
	for (state=Count; state<=Print; ) {
	if (state == Print) {
		ret_buf = (char**)malloc(total + sizeof(char*)*naddr);
		buf = (char*)(ret_buf + naddr);
		len = total;
	}
	while (naddr) {
		if (state == Print)
			ret_buf[naddr-1] = buf;
		pc = addr[naddr-1];

		found = false;
		bfd_map_over_sections(abfd, find_address_in_section,
		(PTR) NULL);

		if (!found) {
			total += snprintf(buf, len, "[0x%llx] \?\?() \?\?:0",(long long unsigned int) addr[naddr-1]) + 1;
		} else {
			const char *name;

			name = functionname;
			if (name == NULL || *name == '\0')
				name = "??";
            else {
#ifdef __cplusplus
                // In C++, demangle the name if it's mangled:
                int status = 0;
                char *d = 0;
                d = abi::__cxa_demangle(name, 0, 0, &status);
                if (d)
                    name = d;
                else
#endif
                // Both in C and C++, if the name is not mangled (this is
                // always the case in C, but sometimes also in C++), append
                // "()" at the end of the string:
                {
                    char *out;
                    asprintf(&out, "%s()", name);
                    name = out;
                }
            }
            /*
			if (filename != NULL) {
				char *h;

				h = (char *) strrchr(filename, '/');
				if (h != NULL)
					filename = h + 1;
			}*/
            char *line_text=NULL;
            if (filename)
                line_text = read_line_from_file(filename, line);
            if (line_text)
                total += snprintf(buf, len, "  File \"%s\", line %u, in %s\n    %s", filename ? filename : "??",
                       line, name, line_text) + 1;
            else
                total += snprintf(buf, len, "  File \"%s\", line %u, in %s", filename ? filename : "??",
                       line, name) + 1;

		}
		if (state == Print) {
			/* set buf just past the end of string */
			buf = buf + total + 1;
		}
		naddr--;
	}
	naddr = naddr_orig;
    if (state == Print)
        break;
    state = Print;
	}
	return ret_buf;
}

#define MAX_DEPTH 16

struct file_match {
	const char *file;
	void *address;
	void *base;
	void *hdr;
};

static int find_matching_file(struct dl_phdr_info *info,
		size_t size, void *data)
{
	struct file_match *match = (struct file_match *)data;
	/* This code is modeled from Gfind_proc_info-lsb.c:callback() from libunwind */
	long n;
	const ElfW(Phdr) *phdr;
	ElfW(Addr) load_base = info->dlpi_addr;
	phdr = info->dlpi_phdr;
	for (n = info->dlpi_phnum; --n >= 0; phdr++) {
		if (phdr->p_type == PT_LOAD) {
			ElfW(Addr) vaddr = phdr->p_vaddr + load_base;
			if ((long unsigned)(match->address) >= vaddr && (long unsigned)(match->address) < vaddr + phdr->p_memsz) {
				/* we found a match */
				match->file = info->dlpi_name;
				match->base = (void*)(info->dlpi_addr);
			}
		}
	}
	return 0;
}

char **backtrace_symbols(void *const *buffer, int size)
{
	int stack_depth = size - 1;
	int x,y;
	/* discard calling function */
	int total = 0;

	char ***locations;
	char **final;
	char *f_strings;

	locations = (char***)malloc(sizeof(char**) * (stack_depth+1));

	bfd_init();
	for(x=stack_depth, y=0; x>=0; x--, y++){
		struct file_match match;
        match.address = buffer[x];
		char **ret_buf;
		bfd_vma addr;
		dl_iterate_phdr(find_matching_file, &match);
		addr = (bfd_vma)((long unsigned)(buffer[x])
                - (long unsigned)(match.base));
		if (match.file && strlen(match.file))
			ret_buf = process_file(match.file, &addr, 1);
		else
			ret_buf = process_file("/proc/self/exe", &addr, 1);
		locations[x] = ret_buf;
		total += strlen(ret_buf[0]) + 1;
	}

	/* allocate the array of char* we are going to return and extra space for
	 * all of the strings */
	final = (char**)malloc(total + (stack_depth + 1) * sizeof(char*));
	/* get a pointer to the extra space */
	f_strings = (char*)(final + stack_depth + 1);

	/* fill in all of strings and pointers */
	for(x=stack_depth; x>=0; x--){
		strcpy(f_strings, locations[x][0]);
		free(locations[x]);
		final[x] = f_strings;
		f_strings += strlen(f_strings) + 1;
	}

	free(locations);

	return final;
}

/* -----------------------------------------------------

Everything below this line is MIT licensed.
*/

/**
 * Read a line from the given file pointer, stripping the traling newline.
 * NULL is returned if an error or EOF is encountered.
 */
#define INIT_BUF_SIZE 64

char *read_line(FILE *fp) {
    int buf_size = INIT_BUF_SIZE;
    char *buf = (char*)calloc(buf_size, sizeof(char)); *buf = 0;
    int tail_size = buf_size;
    char *tail = buf;

    /* successively read portions of the line into the tail of the buffer
     * (the empty section of the buffer following the text that has already
     * been read) until the end of the line is encountered */
    while(!feof(fp)) {
        if(fgets(tail, tail_size, fp) == NULL) {
            /* EOF or read error */
            free(buf);
            return NULL;
        }
        if(tail[strlen(tail)-1] == '\n') {
            /* end of line reached */
            break;
        }
        /* double size of buffer */
        tail_size = buf_size + 1; /* size of new tail */
        buf_size *= 2; /* increase size of buffer to fit new tail */
        buf = (char*)realloc(buf, buf_size * sizeof(char));
        tail = buf + buf_size - tail_size; /* point tail at null-terminator */
    }
    tail[strlen(tail)-1] = 0; /* remove trailing newline */
    return buf;
}

/* Look for an address in a section.  This is called via
   bfd_map_over_sections.  */

static void find_address_in_section(bfd *abfd, asection *section, void *data __attribute__ ((__unused__)) )
{
	bfd_vma vma;
	bfd_size_type size;

	if (found)
		return;

	if ((bfd_get_section_flags(abfd, section) & SEC_ALLOC) == 0)
		return;

	vma = bfd_get_section_vma(abfd, section);
	if (pc < vma)
		return;

	size = bfd_section_size(abfd, section);
	if (pc >= vma + size)
		return;

    // Originally there was "pc-vma", but sometimes the bfd_find_nearest_line
    // returns the next line after the correct one. "pc-vma-1" seems to produce
    // correct line numbers:
	found = bfd_find_nearest_line(abfd, section, syms, pc - vma - 1,
				      &filename, &functionname, &line);
}


/* Loads the symbol table into the global variable 'syms'.  */

static void slurp_symtab(bfd * abfd)
{
	long symcount;
	unsigned int size;

	if ((bfd_get_file_flags(abfd) & HAS_SYMS) == 0)
		return;

	symcount = bfd_read_minisymbols(abfd, false, (void **) &syms, &size);
	if (symcount == 0)
		symcount = bfd_read_minisymbols(abfd, true /* dynamic */ ,
						(void **) &syms, &size);

	if (symcount < 0)
        fatal("bfd_read_minisymbols() failed");
}


/* Process a file.  */

static char **process_file(const char *file_name, bfd_vma *addr, int naddr)
{
    // Initialize 'abfd' and do some sanity checks
	bfd *abfd;
	abfd = bfd_openr(file_name, NULL);
	if (abfd == NULL)
        fatal("bfd_openr() failed");
	if (bfd_check_format(abfd, bfd_archive))
		fatal("Cannot get addresses from archive");
	char **matching;
	if (!bfd_check_format_matches(abfd, bfd_object, &matching))
        fatal("bfd_check_format_matches() failed");


    // Read the symbols
	slurp_symtab(abfd);

    // Get nice representation of each address
	char **ret_buf;
	ret_buf = translate_addresses_buf(abfd, addr, naddr);

    // cleanup
	if (syms != NULL) {
		free(syms);
		syms = NULL;
	}
	bfd_close(abfd);
	return ret_buf;
}


/*
   Reads the 'line_number'th line from the file filename.
*/
char* read_line_from_file(const char *filename, unsigned int line_number)
{
    FILE *in = fopen(filename, "r");
    if (in == NULL)
        return NULL;
    if (line_number == 0)
        return (char*)"Line number must be positive";
    int n = 0;
    char *line;
    while (n < line_number) {
        n += 1;
        line = read_line(in);
        if (line == NULL) {
            return (char*)"Line not found";
        }
    }
    return line;
}


/* Obtain a backtrace and print it to stdout. */
void show_backtrace (void)
{
  void *array[100];
  size_t size;
  char **strings;
  size_t i;

  // Obtain the list of addresses
  size = backtrace(array, 100);
  // Convert addresses to filenames, line numbers, function names and the line
  // text
  strings = backtrace_symbols(array, size);

  // Print it in a Python like fashion:
  printf ("Traceback (most recent call last):\n", size);
  for (i = 0; i < size; i++)
     printf ("%s\n", strings[size-i-1]);

  free(strings);
}

void _segfault_callback_print_stack(int sig_num)
{
    printf("\nSegfault caught. Printing stacktrace:\n\n");
    show_backtrace();
    printf("\nDone. Exiting the program.\n");
    exit(-1);
}

void print_stack_on_segfault()
{
    signal(SIGSEGV, _segfault_callback_print_stack);
}
