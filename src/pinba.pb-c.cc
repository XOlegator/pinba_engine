#include "pinba.pb-c.h"
#include "pinba.pb.h"

#include <cstring>
#include <algorithm>

namespace {

constexpr std::size_t kDictionaryEntrySize = PINBA_DICTIONARY_ENTRY_SIZE;

template <typename RepeatedField, typename Storage, typename Pointer>
void fill_scalar_array(const RepeatedField &input,
                       Storage &storage,
                       Pointer *&raw_ptr,
                       std::size_t &size_field) {
    storage.assign(input.begin(), input.end());
    raw_ptr = storage.empty() ? nullptr : storage.data();
    size_field = storage.size();
}

bool copy_fixed_string(const std::string &src, char *dest, std::size_t max_len) {
    std::memset(dest, 0, max_len);
    if (src.empty()) {
        return true;
    }
    std::strncpy(dest, src.c_str(), max_len - 1);
    return src.size() < max_len;
}

std::unique_ptr<Pinba__Request> convert_request(const Pinba::Request &proto);

std::unique_ptr<Pinba__Request> convert_request(const Pinba::Request &proto) {
    auto message = std::make_unique<Pinba__Request>();

    copy_fixed_string(proto.hostname(), message->hostname, sizeof(message->hostname));
    copy_fixed_string(proto.server_name(), message->server_name, sizeof(message->server_name));
    copy_fixed_string(proto.script_name(), message->script_name, sizeof(message->script_name));
    copy_fixed_string(proto.schema(), message->schema, sizeof(message->schema));

    message->request_count = proto.request_count();
    message->document_size = proto.document_size();
    message->memory_peak = proto.memory_peak();
    message->request_time = proto.request_time();
    message->ru_utime = proto.ru_utime();
    message->ru_stime = proto.ru_stime();
    message->has_status = proto.has_status();
    message->status = proto.has_status() ? proto.status() : 0;
    message->has_memory_footprint = proto.has_memory_footprint();
    message->memory_footprint = proto.has_memory_footprint() ? proto.memory_footprint() : 0;

    fill_scalar_array(proto.timer_hit_count(),
                      message->timer_hit_count_storage,
                      message->timer_hit_count,
                      message->n_timer_hit_count);

    fill_scalar_array(proto.timer_value(),
                      message->timer_value_storage,
                      message->timer_value,
                      message->n_timer_value);

    fill_scalar_array(proto.timer_tag_count(),
                      message->timer_tag_count_storage,
                      message->timer_tag_count,
                      message->n_timer_tag_count);

    fill_scalar_array(proto.timer_tag_name(),
                      message->timer_tag_name_storage,
                      message->timer_tag_name,
                      message->n_timer_tag_name);

    fill_scalar_array(proto.timer_tag_value(),
                      message->timer_tag_value_storage,
                      message->timer_tag_value,
                      message->n_timer_tag_value);

    fill_scalar_array(proto.tag_name(),
                      message->tag_name_storage,
                      message->tag_name,
                      message->n_tag_name);

    fill_scalar_array(proto.tag_value(),
                      message->tag_value_storage,
                      message->tag_value,
                      message->n_tag_value);

    fill_scalar_array(proto.timer_ru_utime(),
                      message->timer_ru_utime_storage,
                      message->timer_ru_utime,
                      message->n_timer_ru_utime);

    fill_scalar_array(proto.timer_ru_stime(),
                      message->timer_ru_stime_storage,
                      message->timer_ru_stime,
                      message->n_timer_ru_stime);

    message->n_dictionary = proto.dictionary_size();
    if (message->n_dictionary > 0) {
        message->dictionary_storage.assign(message->n_dictionary * kDictionaryEntrySize, '\0');
        for (int i = 0; i < proto.dictionary_size(); ++i) {
            char *entry = message->dictionary_storage.data() + (i * kDictionaryEntrySize);
            std::strncpy(entry, proto.dictionary(i).c_str(), kDictionaryEntrySize - 1);
        }
        message->dictionary = message->dictionary_storage.data();
    } else {
        message->dictionary_storage.clear();
        message->dictionary = nullptr;
    }

    if (proto.requests_size() > 0) {
        message->nested_requests_storage.reserve(proto.requests_size());
        message->nested_requests_view.reserve(proto.requests_size());
        for (const auto &nested_proto : proto.requests()) {
            auto nested_request = convert_request(nested_proto);
            message->nested_requests_view.push_back(nested_request.get());
            message->nested_requests_storage.push_back(std::move(nested_request));
        }
        message->requests = message->nested_requests_view.data();
        message->n_requests = message->nested_requests_view.size();
    }

    return message;
}

} // namespace

Pinba__Request::Pinba__Request() {
    pinba__request__init(this);
}

void pinba__request__init(Pinba__Request *message) {
    if (!message) {
        return;
    }
    std::memset(message->hostname, 0, sizeof(message->hostname));
    std::memset(message->server_name, 0, sizeof(message->server_name));
    std::memset(message->script_name, 0, sizeof(message->script_name));
    std::memset(message->schema, 0, sizeof(message->schema));

    message->request_count = 0;
    message->document_size = 0;
    message->memory_peak = 0;
    message->request_time = 0.0f;
    message->ru_utime = 0.0f;
    message->ru_stime = 0.0f;
    message->has_status = false;
    message->status = 0;
    message->has_memory_footprint = false;
    message->memory_footprint = 0;

    message->n_timer_hit_count = 0;
    message->timer_hit_count = nullptr;
    message->n_timer_value = 0;
    message->timer_value = nullptr;
    message->n_timer_tag_count = 0;
    message->timer_tag_count = nullptr;
    message->n_timer_tag_name = 0;
    message->timer_tag_name = nullptr;
    message->n_timer_tag_value = 0;
    message->timer_tag_value = nullptr;
    message->n_dictionary = 0;
    message->dictionary = nullptr;
    message->n_requests = 0;
    message->requests = nullptr;
    message->n_tag_name = 0;
    message->tag_name = nullptr;
    message->n_tag_value = 0;
    message->tag_value = nullptr;
    message->n_timer_ru_utime = 0;
    message->timer_ru_utime = nullptr;
    message->n_timer_ru_stime = 0;
    message->timer_ru_stime = nullptr;

    message->timer_hit_count_storage.clear();
    message->timer_value_storage.clear();
    message->timer_tag_count_storage.clear();
    message->timer_tag_name_storage.clear();
    message->timer_tag_value_storage.clear();
    message->tag_name_storage.clear();
    message->tag_value_storage.clear();
    message->timer_ru_utime_storage.clear();
    message->timer_ru_stime_storage.clear();
    message->dictionary_storage.clear();
    message->nested_requests_storage.clear();
    message->nested_requests_view.clear();
}

Pinba__Request *pinba__request__unpack(ProtobufCAllocator * /*allocator*/,
                                       size_t len,
                                       const uint8_t *data) {
    if (!data || len == 0) {
        return nullptr;
    }

    Pinba::Request proto;
    if (!proto.ParseFromArray(data, static_cast<int>(len))) {
        return nullptr;
    }

    auto request = convert_request(proto);
    return request.release();
}

void pinba__request__free_unpacked(Pinba__Request *message,
                                   ProtobufCAllocator * /*allocator*/) {
    delete message;
}
