#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef DICT
#define DICT
#endif

    typedef struct nlist
    {
        char *name;
        char *defn;
        struct nlist *next;
    } nlist;

    struct nlist *lookup(char *s);
    struct nlist *install(char *name, char *defn);
    void undef(char *name);
    void replace(char *name);

#ifdef __cplusplus
}
#endif
