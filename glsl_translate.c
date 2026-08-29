#include "glsl_translate.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef struct {
    char *text;   /* original token text */
    int is_id;    /* 1 if identifier, else verbatim (punct/space/number) */
} Token;

static Token *tok_alloc(int *cap) {
    int c = 256;
    Token *t = malloc(sizeof(Token) * c);
    *cap = c;
    return t;
}

static void tok_push(Token **t, int *n, int *cap, const char *s, int len, int is_id) {
    if (*n >= *cap) {
        *cap *= 2;
        *t = realloc(*t, sizeof(Token) * (*cap));
    }
    Token *tk = &(*t)[*n];
    tk->text = malloc(len + 1);
    memcpy(tk->text, s, len);
    tk->text[len] = '\0';
    tk->is_id = is_id;
    (*n)++;
}

static int is_ident_char(char c) {
    return isalnum((unsigned char)c) || c == '_';
}

/* tokenize, splitting identifiers from everything else */
static Token *tokenize(const char *src, int *ntok) {
    int n = 0, cap = 0;
    Token *t = tok_alloc(&cap);
    const char *p = src;
    while (*p) {
        if (is_ident_char(*p)) {
            const char *start = p;
            while (*p && is_ident_char(*p)) p++;
            tok_push(&t, &n, &cap, start, (int)(p - start), 1);
        } else {
            /* a run of non-identifier chars (spaces, newlines, punctuation) */
            const char *start = p;
            while (*p && !is_ident_char(*p)) p++;
            tok_push(&t, &n, &cap, start, (int)(p - start), 0);
        }
    }
    *ntok = n;
    return t;
}

/* true if id equals s */
static int eq(const char *a, const char *s) { return strcmp(a, s) == 0; }

/* declare built-in vertex attributes that desktop GL provides for free
 * but GLSL ES 3.00 removed. Returns appended declarations into 'out'. */
static void emit_builtin_decls(const Token *t, int n, glsl_stage stage, char *buf, size_t *pos, size_t size) {
    int need_vertex = 0, need_normal = 0, need_color = 0, need_sec = 0;
    int need_mtc[8] = {0};
    int need_texcoord = 0, need_fog = 0, need_clipvertex = 0;
    int uses_fragcolor = 0, uses_fragdata = 0, max_fd = 0;
    int uses_fragdepth = 0;

    for (int i = 0; i < n; i++) {
        if (!t[i].is_id) continue;
        const char *s = t[i].text;
        if (eq(s, "gl_Vertex")) need_vertex = 1;
        else if (eq(s, "gl_Normal")) need_normal = 1;
        else if (eq(s, "gl_Color")) need_color = 1;
        else if (eq(s, "gl_SecondaryColor")) need_sec = 1;
        else if (eq(s, "gl_FogCoord")) need_fog = 1;
        else if (eq(s, "gl_TexCoord")) need_texcoord = 1;
        else if (eq(s, "gl_ClipVertex")) need_clipvertex = 1;
        else if (eq(s, "gl_FragColor")) uses_fragcolor = 1;
        else if (eq(s, "gl_FragDepth")) uses_fragdepth = 1;
        else if (eq(s, "gl_FragData")) {
            uses_fragdata = 1;
            /* try to find the index literal after [ */
            if (i + 2 < n && eq(t[i+1].text, "[")) {
                int v = atoi(t[i+2].text);
                if (v > max_fd) max_fd = v;
            }
        }
        for (int k = 0; k < 8; k++) {
            char name[32];
            snprintf(name, sizeof(name), "gl_MultiTexCoord%d", k);
            if (eq(s, name)) need_mtc[k] = 1;
        }
    }

    if (stage == STAGE_VERTEX) {
        if (need_vertex) { snprintf(buf + *pos, size - *pos, "in vec4 gl_Vertex;\n"); *pos += strlen(buf + *pos); }
        if (need_normal) { snprintf(buf + *pos, size - *pos, "in vec3 gl_Normal;\n"); *pos += strlen(buf + *pos); }
        if (need_color)  { snprintf(buf + *pos, size - *pos, "in vec4 gl_Color;\n"); *pos += strlen(buf + *pos); }
        if (need_sec)    { snprintf(buf + *pos, size - *pos, "in vec4 gl_SecondaryColor;\n"); *pos += strlen(buf + *pos); }
        if (need_fog)    { snprintf(buf + *pos, size - *pos, "in float gl_FogCoord;\n"); *pos += strlen(buf + *pos); }
        if (need_texcoord){ snprintf(buf + *pos, size - *pos, "out vec4 gl_TexCoord[8];\n"); *pos += strlen(buf + *pos); }
        if (need_clipvertex){ snprintf(buf + *pos, size - *pos, "out vec4 gl_ClipVertex;\n"); *pos += strlen(buf + *pos); }
        for (int k = 0; k < 8; k++) if (need_mtc[k]) {
            snprintf(buf + *pos, size - *pos, "in vec4 gl_MultiTexCoord%d;\n", k); *pos += strlen(buf + *pos);
        }
    } else {
        if (need_texcoord){ snprintf(buf + *pos, size - *pos, "in vec4 gl_TexCoord[8];\n"); *pos += strlen(buf + *pos); }
        if (uses_fragcolor){ snprintf(buf + *pos, size - *pos, "out vec4 _fragColor;\n"); *pos += strlen(buf + *pos); }
        if (uses_fragdata) {
            snprintf(buf + *pos, size - *pos, "out vec4 gl_FragData[%d];\n", max_fd + 1); *pos += strlen(buf + *pos);
        }
        if (uses_fragdepth) { /* gl_FragDepth is built-in in ES3, nothing to add */ }
    }
}

char *glsl_translate(const char *src, glsl_stage stage) {
    int n = 0;
    Token *t = tokenize(src, &n);

    /* compute output size: worst case ~2x + decls */
    size_t outsize = strlen(src) * 2 + 2048;
    char *out = malloc(outsize);
    size_t pos = 0;

    /* header */
    snprintf(out + pos, outsize - pos, "#version 300 es\n");
    pos += strlen(out + pos);
    if (stage == STAGE_FRAGMENT) {
        snprintf(out + pos, outsize - pos, "precision highp float;\n");
        pos += strlen(out + pos);
        snprintf(out + pos, outsize - pos, "precision highp int;\n");
        pos += strlen(out + pos);
    } else {
        snprintf(out + pos, outsize - pos, "precision highp float;\n");
        pos += strlen(out + pos);
    }

    emit_builtin_decls(t, n, stage, out, &pos, outsize);

    /* scan for #version to skip original */
    int i = 0;

    /* we translate token by token, skipping the original #version line */
    for (i = 0; i < n; i++) {
        if (!t[i].is_id) {
            /* check for #version line: a token starting with '#' */
            if (strncmp(t[i].text, "#", 1) == 0) {
                /* skip tokens until we hit a newline-containing token */
                int j = i;
                while (j < n) {
                    if (strchr(t[j].text, '\n')) { i = j; break; }
                    j++;
                }
                if (i >= n) break;
                /* emit the newline token's trailing part after the newline */
                const char *nl = strchr(t[i].text, '\n');
                if (nl) {
                    /* preserve only what's after the newline */
                    snprintf(out + pos, outsize - pos, "%s", nl + 1);
                    pos += strlen(out + pos);
                }
                continue;
            }
            snprintf(out + pos, outsize - pos, "%s", t[i].text);
            pos += strlen(out + pos);
            continue;
        }

        const char *s = t[i].text;
        const char *repl = NULL;

        if (eq(s, "attribute")) {
            repl = (stage == STAGE_VERTEX) ? "in" : "in";
        } else if (eq(s, "varying")) {
            repl = (stage == STAGE_VERTEX) ? "out" : "in";
        } else if (eq(s, "texture2D") || eq(s, "texture3D") || eq(s, "textureCube")) {
            repl = "texture";
        } else if (eq(s, "texture2DLod") || eq(s, "texture3DLod") || eq(s, "textureCubeLod")) {
            repl = "textureLod";
        } else if (eq(s, "gl_FragColor")) {
            repl = "_fragColor";
        }

        if (repl) {
            snprintf(out + pos, outsize - pos, "%s", repl);
            pos += strlen(out + pos);
        } else {
            snprintf(out + pos, outsize - pos, "%s", s);
            pos += strlen(out + pos);
        }
    }

    /* cleanup */
    for (int k = 0; k < n; k++) free(t[k].text);
    free(t);

    /* trim */
    out[pos] = '\0';
    return out;
}
