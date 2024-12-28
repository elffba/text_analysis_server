#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <math.h>
#include <limits.h>

#define INPUT_CHARACTER_LIMIT 100
#define OUTPUT_CHARACTER_LIMIT 200
#define PORT_NUMBER 60000
#define LEVENSHTEIN_LIST_LIMIT 5
#define DICT_SIZE 5000

const char *DICT_FILE = "basic_english_2000.txt";
char dictionary[DICT_SIZE][INPUT_CHARACTER_LIMIT];
int dict_count = 0;

void clean_string(char *str);
void load_dictionary();
int levenshtein(const char *s1, const char *s2);
void add_word_to_dict(const char *word);
int is_word_in_dict(const char *word);
void process_word(int client_socket, const char *word, int word_index, char *output);
void handle_client(int client_socket);

void clean_string(char *str)
{
    char *start = str;
    while (isspace((unsigned char)*start))
        start++;
    char *end = start + strlen(start) - 1;
    while (end > start && isspace((unsigned char)*end))
        end--;
    *(end + 1) = '\0';
    strcpy(str, start);
    for (char *p = str; *p; p++)
    {
        *p = tolower((unsigned char)*p);
    }
}

void load_dictionary()
{
    FILE *dict_file = fopen(DICT_FILE, "r");
    if (!dict_file)
    {
        perror("Error opening dictionary file");
        exit(1);
    }

    dict_count = 0;
    while (fgets(dictionary[dict_count], sizeof(dictionary[dict_count]), dict_file))
    {
        clean_string(dictionary[dict_count]);
        if (strlen(dictionary[dict_count]) > 0)
        {
            dict_count++;
        }
    }

    fclose(dict_file);
}

int levenshtein(const char *s1, const char *s2)
{
    int len_s1 = strlen(s1);
    int len_s2 = strlen(s2);
    int d[len_s1 + 1][len_s2 + 1];

    for (int i = 0; i <= len_s1; i++)
        d[i][0] = i;
    for (int j = 0; j <= len_s2; j++)
        d[0][j] = j;

    for (int i = 1; i <= len_s1; i++)
    {
        for (int j = 1; j <= len_s2; j++)
        {
            int cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1;
            d[i][j] = fminf(fminf(d[i - 1][j] + 1, d[i][j - 1] + 1), d[i - 1][j - 1] + cost);
        }
    }

    return d[len_s1][len_s2];
}

int is_word_in_dict(const char *word)
{
    char clean_word[INPUT_CHARACTER_LIMIT];
    strncpy(clean_word, word, INPUT_CHARACTER_LIMIT);
    clean_string(clean_word);

    for (int i = 0; i < dict_count; i++)
    {
        if (strcmp(dictionary[i], clean_word) == 0)
        {
            return 1;
        }
    }
    return 0;
}

void add_word_to_dict(const char *word)
{
    char clean_word[INPUT_CHARACTER_LIMIT];
    strncpy(clean_word, word, INPUT_CHARACTER_LIMIT);
    clean_string(clean_word);

    if (is_word_in_dict(clean_word))
    {
        printf("DEBUG: Word '%s' already exists in dictionary.\n", clean_word);
        return;
    }

    printf("DEBUG: Trying to open dictionary file for appending.\n");
    FILE *dict_file = fopen(DICT_FILE, "a");
    if (dict_file)
    {
        if (fprintf(dict_file, "%s\n", clean_word) < 0)
        {
            perror("ERROR: Writing to dictionary file failed");
        }
        else if (fflush(dict_file) != 0)
        {
            perror("ERROR: Flushing dictionary file failed");
        }
        else
        {
            printf("DEBUG: Word '%s' successfully added to dictionary file.\n", clean_word);
            strncpy(dictionary[dict_count], clean_word, INPUT_CHARACTER_LIMIT);
            dict_count++;
        }
        fclose(dict_file);
    }
    else
    {
        perror("ERROR: Unable to open dictionary file for appending");
    }
}

void process_word(int client_socket, const char *word, int word_index, char *output)
{
    char response[OUTPUT_CHARACTER_LIMIT] = "";

    char closest_words[LEVENSHTEIN_LIST_LIMIT][INPUT_CHARACTER_LIMIT];
    int closest_distances[LEVENSHTEIN_LIST_LIMIT];
    int closest_count = 0;
    int min_distance = INT_MAX;

    // Initialize distances to maximum
    for (int i = 0; i < LEVENSHTEIN_LIST_LIMIT; i++)
    {
        closest_distances[i] = INT_MAX;
    }

    // Find the closest matches
    for (int i = 0; i < dict_count; i++)
    {
        int dist = levenshtein(word, dictionary[i]);
        for (int j = 0; j < LEVENSHTEIN_LIST_LIMIT; j++)
        {
            if (dist < closest_distances[j])
            {
                for (int k = LEVENSHTEIN_LIST_LIMIT - 1; k > j; k--)
                {
                    closest_distances[k] = closest_distances[k - 1];
                    strncpy(closest_words[k], closest_words[k - 1], INPUT_CHARACTER_LIMIT);
                }
                closest_distances[j] = dist;
                strncpy(closest_words[j], dictionary[i], INPUT_CHARACTER_LIMIT);
                break;
            }
        }
    }

    // Prepare the response with closest matches
    snprintf(response, sizeof(response), "WORD %02d: %s\nMATCHES: ", word_index, word);
    for (int i = 0; i < LEVENSHTEIN_LIST_LIMIT && closest_distances[i] < INT_MAX; i++)
    {
        char match[INPUT_CHARACTER_LIMIT];
        snprintf(match, sizeof(match), "%s (%d)%s", closest_words[i], closest_distances[i],
                 (i < LEVENSHTEIN_LIST_LIMIT - 1 && closest_distances[i + 1] < INT_MAX) ? ", " : "\n");
        strncat(response, match, sizeof(response) - strlen(response) - 1);
    }

    if (is_word_in_dict(word))
    {
        strncat(response, "Correct word!\n", sizeof(response) - strlen(response) - 1);
        strncat(output, word, OUTPUT_CHARACTER_LIMIT - strlen(output) - 1);
        strncat(output, " ", OUTPUT_CHARACTER_LIMIT - strlen(output) - 1);
        send(client_socket, response, strlen(response), 0);
        return;
    }

    strncat(response, "WORD not in dictionary. Do you want to add this word to dictionary? (y/N): ", sizeof(response) - strlen(response) - 1);
    send(client_socket, response, strlen(response), 0);

    char user_input[3];
    int bytes_received = recv(client_socket, user_input, sizeof(user_input), 0);
    if (bytes_received > 0 && (user_input[0] == 'y' || user_input[0] == 'Y'))
    {
        add_word_to_dict(word);
        strncat(output, word, OUTPUT_CHARACTER_LIMIT - strlen(output) - 1);
    }
    else
    {
        strncat(output, closest_words[0], OUTPUT_CHARACTER_LIMIT - strlen(output) - 1);
    }
    strncat(output, " ", OUTPUT_CHARACTER_LIMIT - strlen(output) - 1);
}

void handle_client(int client_socket)
{
    char input[INPUT_CHARACTER_LIMIT + 10]; // Fazladan alan (satır sonu karakterleri dahil)
    char output[OUTPUT_CHARACTER_LIMIT] = "";
    char original_input[INPUT_CHARACTER_LIMIT + 1];
    send(client_socket, "Welcome to Text Analysis Server!\nEnter your string:\n", 55, 0);

    int read_size = recv(client_socket, input, sizeof(input) - 1, 0);
    if (read_size <= 0)
    {
        close(client_socket);
        return;
    }

    input[read_size] = '\0';

    // Satır sonu karakterlerini kaldır
    char *newline_pos = strpbrk(input, "\r\n");
    if (newline_pos)
    {
        *newline_pos = '\0';
    }

    // İlk uzunluk kontrolü
    if (strlen(input) > INPUT_CHARACTER_LIMIT)
    {
        send(client_socket, "ERROR: Input string is longer than allowed limit!\n", 48, 0);
        close(client_socket);
        return;
    }

    strncpy(original_input, input, INPUT_CHARACTER_LIMIT);

    // Girdiyi temizle ve lowercase dönüşümünü uygula
    clean_string(input);

    // Orijinal input uppercase içeriyorsa bilgi mesajı gönder
    int contains_uppercase = 0;
    for (int i = 0; i < strlen(original_input); i++)
    {
        if (isupper(original_input[i]))
        {
            contains_uppercase = 1;
            break;
        }
    }

    if (contains_uppercase)
    {
        send(client_socket, "INFO: Input contained uppercase letters. Converted to lowercase.\n", 64, 0);
    }

    // Temizlenmiş verinin uzunluğunu tekrar kontrol et
    if (strlen(input) > INPUT_CHARACTER_LIMIT)
    {
        send(client_socket, "ERROR: Cleaned input string is longer than allowed limit!\n", 55, 0);
        close(client_socket);
        return;
    }

    // Desteklenmeyen karakter kontrolü
    for (int i = 0; i < strlen(input); i++)
    {
        if (!isalpha(input[i]) && !isspace(input[i]))
        {
            send(client_socket, "ERROR: Input string contains unsupported characters!\n", 55, 0);
            close(client_socket);
            return;
        }
    }

    // Tokenlere böl ve işle
    char *token = strtok(input, " ");
    int word_index = 1;

    while (token != NULL)
    {
        process_word(client_socket, token, word_index, output);
        token = strtok(NULL, " ");
        word_index++;
    }

    char final_message[OUTPUT_CHARACTER_LIMIT * 2];
    snprintf(final_message, sizeof(final_message), "\nINPUT: %s\nOUTPUT: %s\n", original_input, output);
    send(client_socket, final_message, strlen(final_message), 0);

    close(client_socket);
}

int main()
{
    load_dictionary();

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0)
    {
        perror("Socket creation failed");
        return 1;
    }

    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT_NUMBER);
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

    printf("Server is running on port %d\n", PORT_NUMBER);

    while (1)
    {
        int client_socket = accept(server_socket, NULL, NULL);
        if (client_socket < 0)
        {
            perror("Accept failed");
            continue;
        }
        handle_client(client_socket);
    }

    close(server_socket);
    return 0;
}
