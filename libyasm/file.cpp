//
// File helper functions.
//
//  Copyright (C) 2001-2007  Peter Johnson
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND OTHER CONTRIBUTORS ``AS IS''
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR OTHER CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
#include <util.h>

// Need either unistd.h or direct.h (on Windows) to prototype getcwd().
#if defined(HAVE_UNISTD_H)
#include <unistd.h>
#elif defined(HAVE_DIRECT_H)
#include <direct.h>
#endif

#include <cctype>

#include "errwarn.h"
#include "file.h"


namespace yasm {

bool
Scanner::fill_helper(unsigned char* &cursor,
                     boost::function<size_t (unsigned char*, size_t)> input_func)
{
    static const size_t BSIZE = 8192;       // Fill block size
    bool first = false;

    if (eof)
        return 0;

    size_t cnt = tok - bot;
    if (cnt > 0) {
        memmove(bot, tok, (size_t)(lim - tok));
        tok = bot;
        ptr -= cnt;
        cursor -= cnt;
        lim -= cnt;
    }
    if (!bot)
        first = true;
    if ((size_t)(top - lim) < BSIZE) {
        unsigned char *buf = new unsigned char[(size_t)(lim - bot) + BSIZE];
        memcpy(buf, tok, (size_t)(lim - tok));
        tok = buf;
        ptr = &buf[ptr - bot];
        cursor = &buf[cursor - bot];
        lim = &buf[lim - bot];
        top = &lim[BSIZE];
        delete bot;
        bot = buf;
    }
    if ((cnt = input_func(lim, BSIZE)) == 0) {
        eof = &lim[cnt];
        *eof++ = '\n';
    }
    lim += cnt;
    return first;
}

void
unescape_cstring(unsigned char* str, size_t &len)
{
    unsigned char* s = str;
    unsigned char* o = str;
    unsigned char t[4];

    while ((size_t)(s-str)<len) {
        if (*s == '\\' && (size_t)(&s[1]-str)<len) {
            s++;
            switch (*s) {
                case 'b': *o = '\b'; s++; break;
                case 'f': *o = '\f'; s++; break;
                case 'n': *o = '\n'; s++; break;
                case 'r': *o = '\r'; s++; break;
                case 't': *o = '\t'; s++; break;
                case 'x':
                    // hex escape; grab last two digits
                    s++;
                    while ((size_t)(&s[2]-str)<len && isxdigit(s[0])
                           && isxdigit(s[1]) && isxdigit(s[2]))
                        s++;
                    if ((size_t)(s-str)<len && isxdigit(*s)) {
                        t[0] = *s++;
                        t[1] = '\0';
                        t[2] = '\0';
                        if ((size_t)(s-str)<len && isxdigit(*s))
                            t[1] = *s++;
                        *o = (unsigned char)strtoul((char *)t, NULL, 16);
                    } else
                        *o = '\0';
                    break;
                default:
                    if (isdigit(*s)) {
                        int warn = 0;
                        // octal escape
                        if (*s > '7')
                            warn = 1;
                        *o = *s++ - '0';
                        if ((size_t)(s-str)<len && isdigit(*s)) {
                            if (*s > '7')
                                warn = 1;
                            *o <<= 3;
                            *o += *s++ - '0';
                            if ((size_t)(s-str)<len && isdigit(*s)) {
                                if (*s > '7')
                                    warn = 1;
                                *o <<= 3;
                                *o += *s++ - '0';
                            }
                        }
                        if (warn)
                            warn_set(WARN_GENERAL, N_("octal value out of range"));
                    } else
                        *o = *s++;
                    break;
            }
            o++;
        } else
            *o++ = *s++;
    }
    len = o-str;
}

size_t
splitpath_unix(const char* path, /*@out@*/ const char* &tail)
{
    const char* s;
    s = strrchr(path, '/');
    if (!s) {
        // No head
        tail = path;
        return 0;
    }
    tail = s+1;
    // Strip trailing ./ on path
    while ((s-1)>=path && *(s-1) == '.' && *s == '/'
           && !((s-2)>=path && *(s-2) == '.'))
        s -= 2;
    // Strip trailing slashes on path (except leading)
    while (s>path && *s == '/')
        s--;
    // Return length of head
    return s-path+1;
}

size_t
splitpath_win(const char* path, /*@out@*/ const char* &tail)
{
    const char* basepath = path;
    const char* s;

    // split off drive letter first, if any
    if (isalpha(path[0]) && path[1] == ':')
        basepath += 2;

    s = basepath;
    while (*s != '\0')
        s++;
    while (s >= basepath && *s != '\\' && *s != '/')
        s--;
    if (s < basepath) {
        tail = basepath;
        if (path == basepath)
            return 0;   // No head
        else
            return 2;   // Drive letter is head
    }
    tail = s+1;
    // Strip trailing .\ or ./ on path
    while ((s-1)>=basepath && *(s-1) == '.' && (*s == '/' || *s == '\\')
           && !((s-2)>=basepath && *(s-2) == '.'))
        s -= 2;
    // Strip trailing slashes on path (except leading)
    while (s>basepath && (*s == '/' || *s == '\\'))
        s--;
    // Return length of head
    return s-path+1;
}

// FIXME: dumb way for now
char *
abspath_unix(const char* path)
{
    char *curdir, *abspath;
    static const char pathsep[2] = "/";

    curdir = getcwd(NULL, 0);

    abspath = new char[strlen(curdir) + strlen(path) + 2];
    strcpy(abspath, curdir);
    strcat(abspath, pathsep);
    strcat(abspath, path);

    free(curdir);

    return abspath;
}

// FIXME: dumb way for now
char *
abspath_win(const char *path)
{
    char *curdir, *abspath, *ch;
    static const char pathsep[2] = "\\";

    curdir = getcwd(NULL, 0);

    abspath = new char[strlen(curdir) + strlen(path) + 2];
    strcpy(abspath, curdir);
    strcat(abspath, pathsep);
    strcat(abspath, path);

    free(curdir);

    // Replace all "/" with "\".
    ch = abspath;
    while (*ch) {
        if (*ch == '/')
            *ch = '\\';
        ch++;
    }

    return abspath;
}

char *
combpath_unix(const char* from, const char* to)
{
    const char* tail;
    size_t pathlen, i, j;
    char* out;

    if (to[0] == '/') {
        // absolute "to"
        out = new char[strlen(to)+1];
        // Combine any double slashes when copying
        for (j=0; *to; to++) {
            if (*to == '/' && *(to+1) == '/')
                continue;
            out[j++] = *to;
        }
        out[j++] = '\0';
        return out;
    }

    // Get path component; note this strips trailing slash
    pathlen = splitpath_unix(from, tail);

    out = new char [pathlen+strlen(to)+2];  // worst case maximum len

    // Combine any double slashes when copying
    for (i=0, j=0; i<pathlen; i++) {
        if (i<pathlen-1 && from[i] == '/' && from[i+1] == '/')
            continue;
        out[j++] = from[i];
    }
    pathlen = j;

    // Add trailing slash back in
    if (pathlen > 0 && out[pathlen-1] != '/')
        out[pathlen++] = '/';

    // Now scan from left to right through "to", stripping off "." and "..";
    // if we see "..", back up one directory in out unless last directory in
    // out is also "..".
    //
    // Note this does NOT back through ..'s in the "from" path; this is just
    // as well as that could skip symlinks (e.g. "foo/bar/.." might not be
    // the same as "foo").
    for (;;) {
        if (to[0] == '.' && to[1] == '/') {
            to += 2;        // current directory
            while (*to == '/')
                to++;           // strip off any additional slashes
        } else if (pathlen == 0)
            break;          // no more "from" path left, we're done
        else if (to[0] == '.' && to[1] == '.' && to[2] == '/') {
            if (pathlen >= 3 && out[pathlen-1] == '/' && out[pathlen-2] == '.'
                && out[pathlen-3] == '.') {
                // can't ".." against a "..", so we're done.
                break;
            }

            to += 3;    // throw away "../"
            while (*to == '/')
                to++;           // strip off any additional slashes

            // and back out last directory in "out" if not already at root
            if (pathlen > 1) {
                pathlen--;      // strip off trailing '/'
                while (pathlen > 0 && out[pathlen-1] != '/')
                    pathlen--;
            }
        } else
            break;
    }

    // Copy "to" to tail of output, and we're done
    // Combine any double slashes when copying
    for (j=pathlen; *to; to++) {
        if (*to == '/' && *(to+1) == '/')
            continue;
        out[j++] = *to;
    }
    out[j++] = '\0';

    return out;
}

char *
combpath_win(const char* from, const char* to)
{
    const char *tail;
    size_t pathlen, i, j;
    char *out;

    if ((isalpha(to[0]) && to[1] == ':') || (to[0] == '/' || to[0] == '\\')) {
        // absolute or drive letter "to"
        out = new char[strlen(to)+1];
        // Combine any double slashes when copying
        for (j=0; *to; to++) {
            if ((*to == '/' || *to == '\\')
                && (*(to+1) == '/' || *(to+1) == '\\'))
                continue;
            if (*to == '/')
                out[j++] = '\\';
            else
                out[j++] = *to;
        }
        out[j++] = '\0';
        return out;
    }

    // Get path component; note this strips trailing slash
    pathlen = splitpath_win(from, tail);

    out = new char[pathlen+strlen(to)+2];   // worst case maximum len

    // Combine any double slashes when copying
    for (i=0, j=0; i<pathlen; i++) {
        if (i<pathlen-1 && (from[i] == '/' || from[i] == '\\')
            && (from[i+1] == '/' || from[i+1] == '\\'))
            continue;
        if (from[i] == '/')
            out[j++] = '\\';
        else
            out[j++] = from[i];
    }
    pathlen = j;

    // Add trailing slash back in, unless it's only a raw drive letter
    if (pathlen > 0 && out[pathlen-1] != '\\'
        && !(pathlen == 2 && isalpha(out[0]) && out[1] == ':'))
        out[pathlen++] = '\\';

    // Now scan from left to right through "to", stripping off "." and "..";
    // if we see "..", back up one directory in out unless last directory in
    // out is also "..".
    //
    // Note this does NOT back through ..'s in the "from" path; this is just
    // as well as that could skip symlinks (e.g. "foo/bar/.." might not be
    // the same as "foo").
    for (;;) {
        if (to[0] == '.' && (to[1] == '/' || to[1] == '\\')) {
            to += 2;        // current directory
            while (*to == '/' || *to == '\\')
                to++;           // strip off any additional slashes
        } else if (pathlen == 0
                 || (pathlen == 2 && isalpha(out[0]) && out[1] == ':'))
            break;          // no more "from" path left, we're done
        else if (to[0] == '.' && to[1] == '.'
                 && (to[2] == '/' || to[2] == '\\')) {
            if (pathlen >= 3 && out[pathlen-1] == '\\'
                && out[pathlen-2] == '.' && out[pathlen-3] == '.') {
                // can't ".." against a "..", so we're done.
                break;
            }

            to += 3;    // throw away "../" (or "..\")
            while (*to == '/' || *to == '\\')
                to++;           // strip off any additional slashes

            // and back out last directory in "out" if not already at root
            if (pathlen > 1) {
                pathlen--;      // strip off trailing '/'
                while (pathlen > 0 && out[pathlen-1] != '\\')
                    pathlen--;
            }
        } else
            break;
    }

    // Copy "to" to tail of output, and we're done
    // Combine any double slashes when copying
    for (j=pathlen; *to; to++) {
        if ((*to == '/' || *to == '\\') && (*(to+1) == '/' || *(to+1) == '\\'))
            continue;
        if (*to == '/')
            out[j++] = '\\';
        else
            out[j++] = *to;
    }
    out[j++] = '\0';

    return out;
}
#if 0
typedef struct incpath {
    STAILQ_ENTRY(incpath) link;
    /*@owned@*/ char *path;
} incpath;

STAILQ_HEAD(, incpath) incpaths = STAILQ_HEAD_INITIALIZER(incpaths);

FILE *
yasm_fopen_include(const char *iname, const char *from, const char *mode,
                   char **oname)
{
    FILE *f;
    char *combine;
    incpath *np;

    // Try directly relative to from first, then each of the include paths.
    if (from) {
        combine = yasm__combpath(from, iname);
        f = fopen(combine, mode);
        if (f) {
            if (oname)
                *oname = combine;
            else
                yasm_xfree(combine);
            return f;
        }
        yasm_xfree(combine);
    }

    STAILQ_FOREACH(np, &incpaths, link) {
        combine = yasm__combpath(np->path, iname);
        f = fopen(combine, mode);
        if (f) {
            if (oname)
                *oname = combine;
            else
                yasm_xfree(combine);
            return f;
        }
        yasm_xfree(combine);
    }

    if (oname)
        *oname = NULL;
    return NULL;
}

void
yasm_delete_include_paths(void)
{
    incpath *n1, *n2;

    n1 = STAILQ_FIRST(&incpaths);
    while (n1) {
        n2 = STAILQ_NEXT(n1, link);
        yasm_xfree(n1->path);
        yasm_xfree(n1);
        n1 = n2;
    }
    STAILQ_INIT(&incpaths);
}

void
yasm_add_include_path(const char *path)
{
    incpath *np = yasm_xmalloc(sizeof(incpath));
    size_t len = strlen(path);

    np->path = yasm_xmalloc(len+2);
    memcpy(np->path, path, len+1);
    // Add trailing slash if it is missing.
    if (path[len-1] != '\\' && path[len-1] != '/') {
        np->path[len] = '/';
        np->path[len+1] = '\0';
    }

    STAILQ_INSERT_TAIL(&incpaths, np, link);
}
#endif
} // namespace yasm
