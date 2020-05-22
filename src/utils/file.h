#ifdef __cplusplus
extern "C"
{
#endif

    size_t getline(char **lineptr, size_t *n, FILE *stream);
    int fcopy(const char *to, const char *from);

#ifdef __cplusplus
}
#endif
