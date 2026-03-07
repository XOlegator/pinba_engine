#ifndef PINBA_PROTOBUF_COMPAT_INCLUDED
#define PINBA_PROTOBUF_COMPAT_INCLUDED

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>
#include <array>
#include "pinba_limits.h"

struct ProtobufCAllocator {
    void *allocator_data;
    void *(*alloc)(void *allocator_data, size_t size);
    void (*free)(void *allocator_data, void *pointer);
};

using protobuf_c_boolean = bool;

struct Pinba__Request {
    char hostname[PINBA_HOSTNAME_SIZE];
    char server_name[PINBA_SERVER_NAME_SIZE];
    char script_name[PINBA_SCRIPT_NAME_SIZE];
    uint32_t request_count;
    uint32_t document_size;
    uint32_t memory_peak;
    float request_time;
    float ru_utime;
    float ru_stime;

    size_t n_timer_hit_count;
    uint32_t *timer_hit_count;
    size_t n_timer_value;
    float *timer_value;
    size_t n_timer_tag_count;
    uint32_t *timer_tag_count;
    size_t n_timer_tag_name;
    uint32_t *timer_tag_name;
    size_t n_timer_tag_value;
    uint32_t *timer_tag_value;

    size_t n_dictionary;
    char *dictionary;
    protobuf_c_boolean has_status;
    uint32_t status;
    protobuf_c_boolean has_memory_footprint;
    uint32_t memory_footprint;

    size_t n_requests;
    Pinba__Request **requests;

    char schema[PINBA_SCHEMA_SIZE];
    size_t n_tag_name;
    uint32_t *tag_name;
    size_t n_tag_value;
    uint32_t *tag_value;
    size_t n_timer_ru_utime;
    float *timer_ru_utime;
    size_t n_timer_ru_stime;
    float *timer_ru_stime;

    std::vector<uint32_t> timer_hit_count_storage;
    std::vector<float> timer_value_storage;
    std::vector<uint32_t> timer_tag_count_storage;
    std::vector<uint32_t> timer_tag_name_storage;
    std::vector<uint32_t> timer_tag_value_storage;
    std::vector<uint32_t> tag_name_storage;
    std::vector<uint32_t> tag_value_storage;
    std::vector<float> timer_ru_utime_storage;
    std::vector<float> timer_ru_stime_storage;
    std::vector<char> dictionary_storage;
    std::vector<std::unique_ptr<Pinba__Request>> nested_requests_storage;
    std::vector<Pinba__Request*> nested_requests_view;

    Pinba__Request();
};

void pinba__request__init(Pinba__Request *message);
Pinba__Request *pinba__request__unpack(ProtobufCAllocator *allocator,
                                       size_t len,
                                       const uint8_t *data);
void pinba__request__free_unpacked(Pinba__Request *message,
                                   ProtobufCAllocator *allocator);

#endif /* PINBA_PROTOBUF_COMPAT_INCLUDED */
