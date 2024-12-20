#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>  // fminf fonksiyonu için
#include <ctype.h> // isalpha fonksiyonu için

// Levşnstein fonksiyonu burada tanımlanacak
int levenshtein(char *s1, char *s2)
{
    int len_s1 = strlen(s1);
    int len_s2 = strlen(s2);
    int d[len_s1 + 1][len_s2 + 1];
    int i, j, cost;

    for (i = 0; i <= len_s1; i++)
        d[i][0] = i;
    for (j = 0; j <= len_s2; j++)
        d[0][j] = j;

    for (i = 1; i <= len_s1; i++)
    {
        for (j = 1; j <= len_s2; j++)
        {
            cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1;
            d[i][j] = fminf(fminf(d[i - 1][j] + 1, d[i][j - 1] + 1), d[i - 1][j - 1] + cost);
        }
    }

    return d[len_s1][len_s2];
}
