#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <math.h>  // fmin fonksiyonu için
#include <ctype.h> // isalpha fonksiyonu için

// Levşnstein fonksiyonunun prototipi
int levenshtein(char *s1, char *s2);

#define PORT 60000
#define INPUT_CHARACTER_LIMIT 100
#define LEVENSHTEIN_LIST_LIMIT 5

// Dosya okuma ve kelime ekleme fonksiyonları
int is_word_in_dict(const char *word);
void add_word_to_dict(const char *word);

// İstemci isteğini işleyen thread fonksiyonu
void *client_handler(void *arg)
{
    int client_socket = *(int *)arg;
    free(arg);
    char word[INPUT_CHARACTER_LIMIT];
    char dict_word[100];
    char closest_words[LEVENSHTEIN_LIST_LIMIT][100];
    int min_distance = 1000;

    FILE *dict_file = fopen("basic_english_2000.txt", "r");
    if (dict_file == NULL)
    {
        perror("Error opening dictionary file");
        close(client_socket);
        return NULL;
    }

    while (1)
    {
        int read_size = recv(client_socket, word, sizeof(word) - 1, 0);
        if (read_size <= 0)
        {
            printf("Connection closed or error occurred.\n");
            break;
        }
        word[read_size] = '\0';
        word[strcspn(word, "\r\n")] = '\0';

        if (strlen(word) > INPUT_CHARACTER_LIMIT)
        {
            send(client_socket, "ERROR: Input string is longer than 100 characters.\n", 52, 0);
            close(client_socket); // Bağlantıyı kapat
            return NULL;
        }

        for (int i = 0; word[i] != '\0'; i++)
        {
            if (!(isalpha(word[i]) || word[i] == ' '))
            {
                send(client_socket, "ERROR: Input string contains unsupported characters.\n", 56, 0);
                close(client_socket); // Bağlantıyı kapat
                return NULL;
            }
            if (!isascii(word[i]))
            {
                send(client_socket, "ERROR: Input string contains unsupported characters.\n", 56, 0);
                close(client_socket); // Bağlantıyı kapat
                return NULL;
            }
        }

        // Sözlükten kelimeleri karşılaştır
        fseek(dict_file, 0, SEEK_SET);
        int found = 0;
        min_distance = 1000;
        int closest_count = 0;

        while (fgets(dict_word, sizeof(dict_word), dict_file))
        {
            dict_word[strcspn(dict_word, "\r\n")] = '\0';

            if (strcmp(word, dict_word) == 0)
            {
                send(client_socket, "Correct word!\n", 14, 0);
                found = 1;
                break;
            }

            int dist = levenshtein(word, dict_word);
            if (dist < min_distance)
            {
                min_distance = dist;
                closest_count = 0;
                strcpy(closest_words[closest_count++], dict_word);
            }
            else if (dist == min_distance && closest_count < LEVENSHTEIN_LIST_LIMIT)
            {
                strcpy(closest_words[closest_count++], dict_word);
            }
        }

        if (!found)
        {
            char response[500] = "Word not found. Closest matches: ";
            for (int i = 0; i < closest_count; i++)
            {
                strcat(response, closest_words[i]);
                if (i < closest_count - 1)
                    strcat(response, ", ");
            }
            strcat(response, "\n");
            send(client_socket, response, strlen(response), 0);

            // Kelime ekleme veya değiştirme
            char user_input[3];
            send(client_socket, "Do you want to add this word to dictionary? (y/N): ", 51, 0);
            recv(client_socket, user_input, sizeof(user_input), 0);

            if (user_input[0] == 'y' || user_input[0] == 'Y')
            {
                // Kelimeyi sözlüğe ekle
                add_word_to_dict(word);
            }
            else
            {
                // En yakın kelimeyi kullan
                send(client_socket, "Using closest match.\n", 21, 0);
            }
        }
    }

    fclose(dict_file);
    close(client_socket);
    return NULL;
}

// Kelimenin sözlükte olup olmadığını kontrol et
int is_word_in_dict(const char *word)
{
    FILE *dict_file = fopen("basic_english_2000.txt", "r");
    if (dict_file == NULL)
    {
        perror("Error opening dictionary file");
        return 0;
    }

    char dict_word[100];
    while (fgets(dict_word, sizeof(dict_word), dict_file))
    {
        dict_word[strcspn(dict_word, "\n")] = '\0';
        if (strcmp(word, dict_word) == 0)
        {
            fclose(dict_file);
            return 1;
        }
    }

    fclose(dict_file);
    return 0;
}

// Yeni kelimeyi sözlüğe ekle
void add_word_to_dict(const char *word)
{
    if (is_word_in_dict(word))
    {
        printf("The word is already in the dictionary.\n");
        return;
    }

    FILE *dict_file_append = fopen("basic_english_2000.txt", "a");
    if (dict_file_append != NULL)
    {
        fprintf(dict_file_append, "%s\n", word);
        fclose(dict_file_append);
    }
    else
    {
        perror("Error opening dictionary file for appending");
    }
}

int main()
{
    int server_socket, *new_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    pthread_t thread_id;

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1)
    {
        perror("Socket creation failed");
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Bind failed");
        close(server_socket);
        return 1;
    }

    if (listen(server_socket, 3) < 0)
    {
        perror("Listen failed");
        close(server_socket);
        return 1;
    }

    printf("Server listening on port %d...\n", PORT);

    while (1)
    {
        new_sock = malloc(sizeof(int));
        *new_sock = accept(server_socket, (struct sockaddr *)&client_addr, &client_addr_len);
        if (*new_sock < 0)
        {
            perror("Accept failed");
            free(new_sock);
            continue;
        }

        printf("Connection accepted from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        if (pthread_create(&thread_id, NULL, client_handler, (void *)new_sock) < 0)
        {
            perror("Thread creation failed");
            free(new_sock);
            continue;
        }
        pthread_detach(thread_id);
    }

    close(server_socket);
    return 0;
}