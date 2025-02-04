#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <math.h>
#include <pthread.h>
#include <limits.h>

// Mutex to prevent race conditions when accessing shared resources
pthread_mutex_t lock;

// Constants to define input/output limits and other server configurations
#define INPUT_CHARACTER_LIMIT 30
#define OUTPUT_CHARACTER_LIMIT 200
#define PORT_NUMBER 60000
#define LEVENSHTEIN_LIST_LIMIT 5
#define DICT_SIZE 5000

// Path to the dictionary file
const char *DICT_FILE = "basic_english_2000.txt";

// Dictionary array to store words and a counter for the number of words
char dictionary[DICT_SIZE][INPUT_CHARACTER_LIMIT];
int dict_count = 0;

// Struct to pass data to threads
typedef struct
{
    char word[INPUT_CHARACTER_LIMIT];    // The word to process
    int word_index;                      // The index of the word in the input
    char result[OUTPUT_CHARACTER_LIMIT]; // Result buffer
    int client_socket;                   // Client socket for communication
} ThreadData;

// Global array to store results of processed words
char results[INPUT_CHARACTER_LIMIT][OUTPUT_CHARACTER_LIMIT];

// Function declarations
void clean_string(char *str);
void load_dictionary();
int levenshtein(const char *s1, const char *s2);
int is_word_in_dict(const char *word);
void add_word_to_dict(const char *word);
void *process_word_thread(void *arg);

// Removes leading/trailing spaces and converts the string to lowercase
void clean_string(char *str)
{
    char *start = str;
    while (isspace((unsigned char)*start)) // Skip spaces at the beginning
        start++;

    char *end = start + strlen(start) - 1;
    while (end > start && isspace((unsigned char)*end)) // Skip spaces at the end
        end--;

    *(end + 1) = '\0';  // Null-terminate the cleaned string
    strcpy(str, start); // Copy cleaned string back

    // Convert all characters to lowercase
    for (char *p = str; *p; p++)
    {
        *p = tolower((unsigned char)*p);
    }
}

// Loads the dictionary from the file into the dictionary array
void load_dictionary()
{
    FILE *dict_file = fopen(DICT_FILE, "r");
    if (!dict_file)
    {
        perror("ERROR: Dictionary file not found!");
        exit(1);
    }

    dict_count = 0;
    while (fgets(dictionary[dict_count], sizeof(dictionary[dict_count]), dict_file))
    {
        clean_string(dictionary[dict_count]); // Clean the line
        if (strlen(dictionary[dict_count]) > 0)
        {
            dict_count++; // Increment the dictionary word count
        }
    }
    fclose(dict_file);
}

// Calculates Levenshtein distance (edit distance) between two strings
int levenshtein(const char *s1, const char *s2)
{
    int len_s1 = strlen(s1);
    int len_s2 = strlen(s2);
    int d[len_s1 + 1][len_s2 + 1];

    // Initialize the distance matrix
    for (int i = 0; i <= len_s1; i++)
        d[i][0] = i; // Cost of deletions
    for (int j = 0; j <= len_s2; j++)
        d[0][j] = j; // Cost of insertions

    // Fill the distance matrix
    for (int i = 1; i <= len_s1; i++)
    {
        for (int j = 1; j <= len_s2; j++)
        {
            int cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1; // Cost of substitution
            d[i][j] = fminf(fminf(d[i - 1][j] + 1, d[i][j - 1] + 1), d[i - 1][j - 1] + cost);
        }
    }

    return d[len_s1][len_s2]; // Return the edit distance
}

// Checks if a word is already in the dictionary
int is_word_in_dict(const char *word)
{
    for (int i = 0; i < dict_count; i++)
    {
        if (strcmp(dictionary[i], word) == 0)
        {
            return 1; // Word found in dictionary
        }
    }
    return 0; // Word not found
}

// Adds a word to the dictionary (both in memory and in the file)
void add_word_to_dict(const char *word)
{
    if (is_word_in_dict(word))
    {
        return; // Don't add duplicates
    }

    FILE *dict_file = fopen(DICT_FILE, "a");
    if (dict_file)
    {
        fprintf(dict_file, "%s\n", word); // Append the word to the file
        fclose(dict_file);
    }

    strncpy(dictionary[dict_count], word, INPUT_CHARACTER_LIMIT); // Add to memory
    dict_count++;                                                 // Update the dictionary count
}

// Processes a single word in a thread
void *process_word_thread(void *arg)
{
    ThreadData *data = (ThreadData *)arg;

    // Convert the word to lowercase (just in case)
    for (int i = 0; data->word[i]; i++)
    {
        data->word[i] = tolower((unsigned char)data->word[i]);
    }

    // Arrays to keep track of closest matches and their distances
    char closest_words[LEVENSHTEIN_LIST_LIMIT][INPUT_CHARACTER_LIMIT];
    int closest_distances[LEVENSHTEIN_LIST_LIMIT];

    // Initialize distances to a very high value
    for (int i = 0; i < LEVENSHTEIN_LIST_LIMIT; i++)
        closest_distances[i] = INT_MAX;

    // Compare the word with each word in the dictionary
    for (int i = 0; i < dict_count; i++)
    {
        int dist = levenshtein(data->word, dictionary[i]);
        for (int j = 0; j < LEVENSHTEIN_LIST_LIMIT; j++)
        {
            if (dist < closest_distances[j])
            {
                // Shift the farther words down
                for (int k = LEVENSHTEIN_LIST_LIMIT - 1; k > j; k--)
                {
                    closest_distances[k] = closest_distances[k - 1];
                    strncpy(closest_words[k], closest_words[k - 1], INPUT_CHARACTER_LIMIT);
                }
                closest_distances[j] = dist;                                     // Update closest distance
                strncpy(closest_words[j], dictionary[i], INPUT_CHARACTER_LIMIT); // Update closest word
                break;
            }
        }
    }

    // Lock mutex to safely update shared results array
    pthread_mutex_lock(&lock);
    snprintf(results[data->word_index - 1], OUTPUT_CHARACTER_LIMIT, "WORD %02d: %s\nMATCHES: ", data->word_index, data->word);
    for (int i = 0; i < LEVENSHTEIN_LIST_LIMIT && closest_distances[i] < INT_MAX; i++)
    {
        snprintf(results[data->word_index - 1] + strlen(results[data->word_index - 1]),
                 OUTPUT_CHARACTER_LIMIT - strlen(results[data->word_index - 1]),
                 "%s (%d)%s", closest_words[i], closest_distances[i],
                 (i < LEVENSHTEIN_LIST_LIMIT - 1 && closest_distances[i + 1] < INT_MAX) ? ", " : "\n");
    }
    pthread_mutex_unlock(&lock); // Unlock mutex

    pthread_exit(NULL); // End the thread
}

// Handles communication with a connected client
void handle_client(int client_socket)
{
    char input[INPUT_CHARACTER_LIMIT];
    char original_input[INPUT_CHARACTER_LIMIT];

    // Send initial message to the client
    send(client_socket, "Hello, this is Text Analysis Server!\nPlease enter your input string:\n", 69, 0);

    // Receive input from the client
    int bytes_received = recv(client_socket, input, sizeof(input), 0);
    if (bytes_received <= 0)
    {
        perror("Connection lost or client disconnected.");
        close(client_socket);
        return;
    }

    input[strcspn(input, "\r\n")] = '\0'; // Remove trailing newlines

    // Check for empty or invalid input
    if (strlen(input) == 0 || strspn(input, " ") == strlen(input))
    {
        send(client_socket, "ERROR: Empty string not allowed.\n", 33, 0);
        close(client_socket);
        return;
    }

    if (strlen(input) >= INPUT_CHARACTER_LIMIT)
    {
        send(client_socket, "ERROR: Input exceeds maximum allowed size.\n", 42, 0);
        close(client_socket);
        return;
    }

    // Check for unsupported characters
    for (int i = 0; i < strlen(input); i++)
    {
        if (!isalpha(input[i]) && !isspace(input[i]))
        {
            send(client_socket, "ERROR: Unsupported characters found. Connection will be closed.\n", 65, 0);
            close(client_socket);
            return;
        }
    }

    strncpy(original_input, input, INPUT_CHARACTER_LIMIT);

    // Convert the input string to lowercase
    for (int i = 0; input[i]; i++)
    {
        input[i] = tolower((unsigned char)input[i]);
    }

    // Prepare threads to process each word in the input
    pthread_t threads[INPUT_CHARACTER_LIMIT];
    ThreadData thread_data[INPUT_CHARACTER_LIMIT];
    int thread_count = 0;

    char *token = strtok(input, " ");
    int word_index = 1;

    while (token != NULL)
    {
        strncpy(thread_data[thread_count].word, token, INPUT_CHARACTER_LIMIT);
        thread_data[thread_count].word_index = word_index;
        thread_data[thread_count].client_socket = client_socket;

        pthread_create(&threads[thread_count], NULL, process_word_thread, &thread_data[thread_count]);
        token = strtok(NULL, " ");
        word_index++;
        thread_count++;
    }

    // Wait for all threads to finish
    for (int i = 0; i < thread_count; i++)
    {
        pthread_join(threads[i], NULL);
    }

    // Send results back to the client
    char final_output[OUTPUT_CHARACTER_LIMIT * INPUT_CHARACTER_LIMIT] = "";
    for (int i = 0; i < thread_count; i++)
    {
        send(client_socket, results[i], strlen(results[i]), 0);

        // If the word is not in the dictionary, ask if it should be added
        if (!is_word_in_dict(thread_data[i].word))
        {
            send(client_socket, "WORD not in dictionary. Do you want to add this word to dictionary? (y/N): ", 73, 0);
            char user_input[3];
            recv(client_socket, user_input, sizeof(user_input), 0);

            if (user_input[0] == 'y' || user_input[0] == 'Y')
            {
                add_word_to_dict(thread_data[i].word);
                strncat(final_output, thread_data[i].word, OUTPUT_CHARACTER_LIMIT - strlen(final_output) - 1);
            }
            else
            {
                char closest_word[INPUT_CHARACTER_LIMIT];
                sscanf(results[i], "WORD %*d: %*s MATCHES: %s", closest_word);
                strncat(final_output, closest_word, OUTPUT_CHARACTER_LIMIT - strlen(final_output) - 1);
            }
        }
        else
        {
            strncat(final_output, thread_data[i].word, OUTPUT_CHARACTER_LIMIT - strlen(final_output) - 1);
        }
        strncat(final_output, " ", OUTPUT_CHARACTER_LIMIT - strlen(final_output) - 1);
    }

    // Send the final transformed output to the client
    char final_message[OUTPUT_CHARACTER_LIMIT * 2];
    snprintf(final_message, sizeof(final_message), "INPUT: %s\nOUTPUT: %s\n", original_input, final_output);
    send(client_socket, final_message, strlen(final_message), 0);

    send(client_socket, "Thank you for using Text Analysis Server! Goodbye!\n", 51, 0);
    close(client_socket);
}

// Main function to set up the server
int main()
{
    // Initialize the mutex
    if (pthread_mutex_init(&lock, NULL) != 0)
    {
        perror("Mutex initialization failed");
        return 1;
    }

    // Load the dictionary into memory
    load_dictionary();

    // Create a socket for the server
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT_NUMBER);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // Bind the socket to the specified port
    bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr));
    listen(server_socket, 3); // Listen for up to 3 connections

    printf("Server is running on port %d\n", PORT_NUMBER);

    // Accept connections and handle clients in a loop
    while (1)
    {
        int client_socket = accept(server_socket, NULL, NULL);
        handle_client(client_socket);
    }

    // Clean up resources
    close(server_socket);
    pthread_mutex_destroy(&lock);
    return 0;
}