#include <arpa/inet.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// #include <sys/endian.h> // geht nicht unter MacOS?
#include <sys/stat.h>
#include <unistd.h>
struct HeaderContent {
  char magic[4];          // ASCII ".snd"
  int32_t data_offset;    // 24 Bytes
  int32_t audio_length;   // 1166076 Bytes
  int32_t encoding_audio; // Typee 2, 8-bit linear PCM
  int32_t sample_rate;    // 44100hz
  int32_t audio_channels; // Mono (1 Channel) oder Stereo
};

void reverse_audio_data(char *audio_data, int length) {
  // In-Place Änderung des Arrays
  // Neues Array nicht notwendig, da es
  // das bestehende Array verändert // geht nicht unter MacOS?
  for (int i = 0; i < length / 2; i++) {
    char temp = audio_data[i];
    audio_data[i] = audio_data[length - i - 1];
    audio_data[length - i - 1] = temp;
  }
}

void convert_back_to_big_endian(struct HeaderContent *header) {
  // be32toh nicht verfügbar in clang 15? weiß nicht wieso
  header->data_offset = htonl(header->data_offset);
  header->audio_length = htonl(header->audio_length);
  header->encoding_audio = htonl(header->encoding_audio);
  header->sample_rate = htonl(header->sample_rate);
  header->audio_channels = htonl(header->audio_channels);
}

int main(int argc, char *argv[]) {

  const char *file_extension = strrchr(argv[1], '.');

  // Input validieren
  if (argc < 2) {
    printf("Please parse the file path as a command line argument.");
    return 1;
  } else if (!file_extension || strcmp(file_extension, ".au") != 0) {
    printf("Invalid file format, only .au file accepted.");
    return 1;
  }

  // File namen auslesen
  char *input = argv[1];

  int fd = open(input, O_RDONLY);

  if (fd < 0) {
    perror("Error opening file");
    return 1;
  }

  // Status: Loaded into memory
  // Header
  struct HeaderContent header_of_au;
  ssize_t bytes_to_read =
      read(fd, &header_of_au, sizeof(struct HeaderContent)); // War 24 hier

  if (bytes_to_read != sizeof(struct HeaderContent)) {
    perror("Error reading header");
    close(fd);
    return 1;
  }

  // Big endian to little endian
  convert_back_to_big_endian(&header_of_au);

  // number of channels
  printf("Number of channels: %d\n", header_of_au.audio_channels);
  // Sampling rate
  printf("Sampling rate: %d\n", header_of_au.sample_rate);
  // Encoding
  printf("Encoding: %d\n", header_of_au.encoding_audio);

  // Audio Length
  printf("Audio Length: %d\n", header_of_au.audio_length);

  // -rev file
  // "-rev\0" daher + 5
  char *output = (char *)malloc(strlen(input) + 5);

  if (output == NULL) {
    perror("Error allocating memory for output");
    close(fd);
    return 1;
  }

  // Hinzufügen von -rev
  sprintf(output, "%.*s-rev.au", (int)(strrchr(input, '.') - input), input);

  // neue file erstellen
  int fd_out = open(output, O_CREAT | O_WRONLY | O_TRUNC, S_IWUSR | S_IRUSR);
  if (fd_out < 0) {
    perror("Error creating output file");
    free(output);
    close(fd);
    return 1;
  }

  // Write the header to the new file
  // vorher zurück zu big endian
  struct HeaderContent big_endian_header = header_of_au;
  convert_back_to_big_endian(&big_endian_header);
  ssize_t header_bytes_written =
      write(fd_out, &big_endian_header, sizeof(struct HeaderContent));
  if (header_bytes_written != sizeof(struct HeaderContent)) {
    perror("Error writing header");
    free(output);
    close(fd);
    close(fd_out);
    return 1;
  }

  // Neue File groß genug machen
  char *audio_data = (char *)malloc(header_of_au.audio_length);

  if (audio_data == NULL) {
    perror("Error allocating memory");
    close(fd);
    return 1;
  }

  ssize_t audio_bytes_read = read(fd, audio_data, header_of_au.audio_length);
  if (audio_bytes_read != header_of_au.audio_length) {
    perror("Error reading audio data. Leength does not match.");
    free(audio_data);
    close(fd);
    return 1;
  }

  // Falls schon reversed
  // Header checken
  int already_reversed = 0;
  char *rev_pos = strstr(input, "-rev");

  if (rev_pos && rev_pos < file_extension) {
    already_reversed = 1;
  }
  struct HeaderContent write_header = header_of_au;
  if (!already_reversed) {
    convert_back_to_big_endian(&write_header);
  }

  // Debug: Print first few bytes before reversing
  /*
  for (int i = 0; i < 10 && i < header_of_au.audio_length; i++) {
    printf("%x ", audio_data[i] & 0xFF);
  }*/
  // printf("\n");

  // if mono, reverse audio
  if (header_of_au.audio_channels == 1) {
    reverse_audio_data(audio_data, header_of_au.audio_length);
  }
  // Debug: Print first few bytes after reversing
  /*
  for (int i = 0; i < 10 && i < header_of_au.audio_length; i++) {
    printf("%x ", audio_data[i] & 0xFF);
  }*/

  // printf("\n");
  // schreiben der Audio Daten in die neue File
  ssize_t audio_bytes_written =
      write(fd_out, audio_data, header_of_au.audio_length);
  if (audio_bytes_written != header_of_au.audio_length) {
    perror("Error writing reversed audio data");
    free(audio_data);
    free(output);
    close(fd);
    close(fd_out);
    return 1;
  }

  printf("Audio data %s and saved to %s.\n",
         already_reversed ? "restored" : "reversed", output);

  free(audio_data);
  free(output);
  close(fd);
  close(fd_out);

  return 0;
}
