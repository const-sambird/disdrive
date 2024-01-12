#define NBDKIT_API_VERSION 2
#include <nbdkit-plugin.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>

#define THREAD_MODEL NBDKIT_THREAD_MODEL_SERIALIZE_ALL_REQUESTS
#define BLOCK_SIZE_IN_BYTES 512
#define NUM_BLOCKS 100
#define DISK_SIZE_IN_BYTES 51200

static void byte_to_ascii(unsigned char input, char* out) {
    out[1] = input & 0x0F;
    out[0] = (input & 0xF0) >> 4;
    for (int i = 0; i < 2; ++i) {
        if (out[i] < 10)
            out[i] += 48; // ASCII '0'
        else
            out[i] += 55; // (ASCII 'A') - 10
    }
}

static void ascii_to_byte(char* input, unsigned char* out) {
    for (int i = 0; i < 2; ++i) {
        if (input[i] < 58)
            input[i] -= 48;
        else
            input[i] -= 55;
    }
    *out = (input[0] << 4) + input[1];
}

/////////////
struct MemoryStruct {
  char *memory;
  size_t size;
};
 
static size_t
WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
  size_t realsize = size * nmemb;
  struct MemoryStruct *mem = (struct MemoryStruct *)userp;
 
  char *ptr = realloc(mem->memory, mem->size + realsize + 1);
  if(!ptr) {
    /* out of memory! */
    printf("not enough memory (realloc returned NULL)\n");
    return 0;
  }
 
  mem->memory = ptr;
  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;
 
  return realsize;
}
////////////

static void disdrive_cleanup() {
    curl_global_cleanup();
}

static int disdrive_after_fork() {
    return 0;
}

static int disdrive_get_ready() {

    printf("get_ready!\n");
    curl_global_init(CURL_GLOBAL_DEFAULT);
    return 0;
}

static void* disdrive_open(int readonly) {
    // FIXME: i don't actually know what a handle does
    printf("open! open! open!\n");
    return NBDKIT_HANDLE_NOT_NEEDED;
}

static int64_t disdrive_get_size(void* handle) {
    return (int64_t) DISK_SIZE_IN_BYTES;
}

static int disdrive_pread(void* handle, void* buf, uint32_t count, uint64_t offset, uint32_t flags) {
    nbdkit_debug("pread %d bytes from %d", count, offset);

    // we're basically reading the whole drive into memory on every read
    // fucking fight me, idk
    CURL* curl = curl_easy_init();
    CURLcode res;
    if (!curl) {
        nbdkit_debug("curl issue");
    }
    struct MemoryStruct chunk;
 
    chunk.memory = malloc(1);  /* will be grown as needed by the realloc above */
    chunk.size = 0;    /* no data at this point */
    char drive[BLOCK_SIZE_IN_BYTES * NUM_BLOCKS * 4];   
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&chunk);
    curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:3000/read");
    nbdkit_debug("preparing to read the drive...");
    res = curl_easy_perform(curl);
    nbdkit_debug("CURLcode: %d", res);
    nbdkit_debug("got %d bytes", chunk.size);
    nbdkit_debug("read the drive!");
    memcpy(drive, chunk.memory, chunk.size);
    uint32_t bytes_remaining = count;
    uint64_t this_offset = offset % BLOCK_SIZE_IN_BYTES;
    uint64_t this_block = offset / BLOCK_SIZE_IN_BYTES;
    uint32_t bytes_this_block;

    while (bytes_remaining > 0) {
        nbdkit_debug("processing read, block %d, %d bytes remain", this_block, bytes_remaining);
        char curr_block_serialised[(BLOCK_SIZE_IN_BYTES / sizeof(char)) * 2];
        memcpy(curr_block_serialised, drive + (BLOCK_SIZE_IN_BYTES * this_block), BLOCK_SIZE_IN_BYTES * 2);
        unsigned char curr_block[BLOCK_SIZE_IN_BYTES];
        for (int i = 0; i < BLOCK_SIZE_IN_BYTES; ++i) {
            ascii_to_byte(curr_block_serialised + (2 * i), &curr_block[i]);
        }
        if (bytes_remaining + this_offset < BLOCK_SIZE_IN_BYTES)
            bytes_this_block = bytes_remaining;
        else
            bytes_this_block = BLOCK_SIZE_IN_BYTES - this_offset;
        memcpy(buf, curr_block + this_offset, bytes_this_block);
        buf += bytes_this_block;
        bytes_remaining -= bytes_this_block;
        this_offset = 0;
        ++this_block;
    }

    curl_easy_cleanup(curl);
    free(chunk.memory);
    nbdkit_debug("read complete!");
    return 0;
}

static int disdrive_pwrite(void* handle, const void* buf, uint32_t count, uint64_t offset, uint32_t flags) {
    printf("pwrite (%i)\n", count);
    nbdkit_debug("buf :");
    char tmp[count];
    memcpy(tmp, buf, count);
    nbdkit_debug("%d", tmp);

    // we're basically reading the whole drive into memory on every read
    // fucking fight me, idk
    CURL* curl = curl_easy_init();
    CURLcode res;
    if (!curl) {
        nbdkit_debug("curl issue");
    }
    struct MemoryStruct chunk;
 
    chunk.memory = malloc(1);  /* will be grown as needed by the realloc above */
    chunk.size = 0;    /* no data at this point */
    char drive[BLOCK_SIZE_IN_BYTES * NUM_BLOCKS * 4];   
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&chunk);
    curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:3000/read");
    res = curl_easy_perform(curl);
    nbdkit_debug("got %d bytes", chunk.size);
    memcpy(drive, chunk.memory, chunk.size);
    uint32_t bytes_remaining = count;
    uint64_t this_offset = offset % BLOCK_SIZE_IN_BYTES;
    int this_block = offset / BLOCK_SIZE_IN_BYTES;
    uint32_t bytes_this_block;
    int blocks_written = count / BLOCK_SIZE_IN_BYTES;
    if (offset != 0)
        blocks_written += 1;

    while (bytes_remaining > 0) {
        nbdkit_debug("processing write, %d bytes remain", bytes_remaining);
        char* curr_block_serialised = (char*) calloc(sizeof(char), 2 * BLOCK_SIZE_IN_BYTES / sizeof(char));
        memcpy(curr_block_serialised, drive + (BLOCK_SIZE_IN_BYTES * this_block * 2), BLOCK_SIZE_IN_BYTES * 2);
        unsigned char curr_block[BLOCK_SIZE_IN_BYTES];
        for (int i = 0; i < BLOCK_SIZE_IN_BYTES; ++i) {
            ascii_to_byte(curr_block_serialised + (2 * i), &curr_block[i]);
        }
        if (bytes_remaining + this_offset < BLOCK_SIZE_IN_BYTES)
            bytes_this_block = bytes_remaining;
        else
            bytes_this_block = BLOCK_SIZE_IN_BYTES - this_offset;
        nbdkit_debug("writing %d bytes in block %d", bytes_this_block, this_block);
        memcpy(curr_block + this_offset, buf, bytes_this_block);
        for (int i = 0; i < BLOCK_SIZE_IN_BYTES; ++i) {
            byte_to_ascii(curr_block[i], curr_block_serialised + (2 * i));
        }
        char querystring[50];
        snprintf(querystring, 50, "http://localhost:3000/write?block=%d&data=", this_block);
        nbdkit_debug("QUERYSTRING: %s", querystring);
        char* uri = (char*) calloc(sizeof(char), strlen(querystring) + strlen(curr_block_serialised) + 1);
        strcpy(uri, querystring);
        strcat(uri, curr_block_serialised);
        CURL* writer = curl_easy_init();
        curl_easy_setopt(writer, CURLOPT_URL, uri);
        curl_easy_perform(writer);
        curl_easy_cleanup(writer);
        buf += bytes_this_block;
        bytes_remaining -= bytes_this_block;
        this_offset = 0;
        ++this_block;
    }

    curl_easy_cleanup(curl);
    free(chunk.memory);
    nbdkit_debug("write complete!");
    return 0;
}

static int disdrive_block_size(void* handle, uint32_t* minimum, uint32_t* preferred, uint32_t* maximum) {
    *minimum = 512;
    *preferred = 512;
    *maximum = 512;
    return 0;
}

static struct nbdkit_plugin plugin = {
    .name = "disdrive",
    .longname = "disdrive - Discord Disk Drive",
    .description = "a FAT12 NBD plugin that stores data on Discord",
    .open = disdrive_open,
    .get_size = disdrive_get_size,
    .pread = disdrive_pread,
    .pwrite = disdrive_pwrite,
    .get_ready = disdrive_get_ready,
    .block_size = disdrive_block_size,
    .after_fork = disdrive_after_fork,
    .cleanup = disdrive_cleanup
};

NBDKIT_REGISTER_PLUGIN(plugin);
