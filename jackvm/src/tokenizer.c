#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <dirent.h>
#include <ascii.h>
#include <tokenizer.h>
#include <tokens.h>
#include <list.h>
 
static char* getword(FILE* stream, char* buff)
{
fstart: {}

    int i = 0; 
    char c;

    buff[0] = '\0'; 

    if ((c = fgetc(stream)) == EOF) return NULL;

    if (isnullchar(c))
    {
        do {
            c = fgetc(stream);
            if (c == EOF) return NULL; 
        } while (isnullchar(c));
    }

    if (issym(c)) 
    {
        do {
            buff[i++] = c; 
        } while (isvalidsym((c = fgetc(stream)), buff, i));
        ungetc(c, stream); 

        goto end; 
    }

    do {
        buff[i++] = c;
    } while (iscmd((c = fgetc(stream))));

end: 
    buff[i] = '\0'; 

    if (iscommentstart(buff)) { skipc(stream, buff); goto fstart; }

    return buff; 
}

unsigned int isstatic(prev, last)
    unsigned short prev, last;
{
    return (prev == PUSH || prev == POP) && (last == STATIC);
}

Source* tokenize(FILE* stream, Source* s)
{
    char buff[300]; 
    unsigned long *labels = s->labels, maxi = 0;

    List* tokens = s->tokens; 
    Hash** cmds = s->cmds; 
    Hash** msegs = s->memsegs; 
    Map* index = s->indexes;

    inline void getconstant(char* word)
    {
        long l = atol(word);
        for (int i = 0; i < 4; i++)
        {
            unsigned short s = 0;
            s |= l >> ((3-i)*16);
            addl(tokens, s); 
        }
    }
    inline unsigned int isconstant(short p, short l)
    {
        return (p == PUSH) && (l == CONSTANT);
    }

    inline int isindex(short p, char* word)
    {
        if (p != PUSH && p != POP && p != FUNCTION && p != CALL) return 0;

        return isnumeral(word);
    }
    
    inline void getspecial(char* word) 
    {
        short prevlast = lasttolast(tokens), last = lastel(tokens), labeli = -1;

        // Check for Memory segment indexes
        if (isconstant(prevlast, last))
        {
            getconstant(word); return;
        }

        if (isindex(prevlast, word))
        {
            short i = atoi(word);
            if (last == STATIC)
            {
                maxi = max(i, maxi); 
                addl(tokens, s->staticcount + i);
            }
            else 
            {
                addl(tokens, i);
            }

            return;
        }

        switch (last)
        {
            case GOTO:
            case IFGOTO:
            case FUNCTION: 
            case CALL: 
            case LABEL: 
            {
                labeli = getmap(index, word); 
                if (labeli == -1) 
                { 
                    labeli = index->counter;
                    add_map(index, word);
                }

                addl(tokens, labeli);
                if (last == FUNCTION || last == LABEL) 
                { 
                    labels[labeli] = listlength(tokens); 
                }

                return;
            }
        } 
    }

    inline void generatetoken(char* word) 
    {
        short token = gethash(cmds, word);
        if (token == -1) token = gethash(msegs, word);
        if (token == -1)
        {
            getspecial(word);
            return;
        }

        addl(tokens, (unsigned short) token);
        return;
    }

    while (getword(stream, buff) != NULL) 
    {
        generatetoken(buff);
    }

    s->staticcount += maxi + 1;

    return s; 

}



Source* tokenizedir(const char* argv, Source* s)
{
    char dirname[200];
    DIR* d;
    unsigned short sources = 0;
   
    get_dirname(dirname, argv);
    d = opendir(dirname);
    struct dirent* de; 
    if (d == NULL)
    {
        fprintf(stderr, "%s is not a valid directory.\n", argv);
        return NULL;
    }

    while ((de = readdir(d)) != NULL)
    {
        if (isvmsource(de->d_name))
        {
            char filename[300];
            FILE* f = NULL;

            get_pathname(filename, dirname, de->d_name);
            f = fopen(filename, "r");

            tokenize(f, s);
            fclose(f);
            sources++;
        }
    }

    closedir(d);

    if (sources == 0) 
    {
        fprintf(stderr, 
                "ERROR: No VM source files in the given directory: '%s'\n",
                dirname);
        exit(1);
    }

    return s;
}



Source *tokenizeall(const char* argv)
{
    Source *s = new_source();
    stdlib(s);

    if (isdir(argv))
    {
        return tokenizedir(argv, s);
    }

    FILE* f = fopen(argv, "r");

    if (f == NULL) return NULL; 

    return tokenize(f, s); 

}

