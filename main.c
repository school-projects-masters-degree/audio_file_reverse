#include <arpa/inet.h> // Include this for byte order conversion
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
  for (int i = 0; i < length / 2; i++) {
    char temp = audio_data[i];
    audio_data[i] = audio_data[length - i - 1];
    audio_data[length - i - 1] = temp;
  }
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

  int fd = open(input, O_RDONLY, O_CREAT | O_EXCL);

  if (fd < 0) {
    perror("Error opening file");
    return 1;
  }

  // Status: Loaded into memory
  // Header
  struct HeaderContent header_of_au;
  ssize_t bytes_to_read = read(fd, &header_of_au, 24);

  if (bytes_to_read != 24) {
    perror("Error reading header");
    close(fd);
  }

  // Big endian to little endian
  // be32toh nicht verfügbar in little endian
  header_of_au.audio_channels = ntohl(header_of_au.audio_channels);
  header_of_au.sample_rate = ntohl(header_of_au.sample_rate);
  header_of_au.encoding_audio = ntohl(header_of_au.encoding_audio);
  header_of_au.audio_length = ntohl(header_of_au.audio_length);

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
  ssize_t header_bytes_written =
      write(fd_out, &header_of_au, sizeof(struct HeaderContent));
  if (header_bytes_written != sizeof(struct HeaderContent)) {
    perror("Error writing header");
    free(output);
    close(fd);
    close(fd_out);
    return 1;
  }
  // if mono, reverse audio
  if (header_of_au.audio_channels == 1) {
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

    // Audio umdrehen
    reverse_audio_data(audio_data, header_of_au.audio_length);

    // schreiben in die neue File
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
    free(audio_data);
    free(output);
    close(fd_out);

    printf("Audio data reversed and saved to %s.\n", output);
  }

  close(fd);

  return 0;
}
