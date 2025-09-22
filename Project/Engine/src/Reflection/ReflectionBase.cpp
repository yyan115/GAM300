#include "pch.h"
#include "Reflection/ReflectionBase.hpp"

// Helper: read a rapidjson::Value into a C++ type T using typed getters.
// CHANGE: rapidjson does not provide a templated Get<T>() for arbitrary types; we centralize conversions here.
template <typename T>
T ReadJsonAs(const rapidjson::Value& v);

template <>
inline bool ReadJsonAs<bool>(const rapidjson::Value& v) { return v.GetBool(); }

template <>
inline int ReadJsonAs<int>(const rapidjson::Value& v)
{
    if (v.IsInt()) return v.GetInt();
    if (v.IsInt64()) return static_cast<int>(v.GetInt64());
    if (v.IsDouble()) return static_cast<int>(v.GetDouble());
    throw std::runtime_error("JSON value is not an integer (int)");
}

template <>
inline unsigned ReadJsonAs<unsigned>(const rapidjson::Value& v)
{
    if (v.IsUint()) return v.GetUint();
    if (v.IsUint64()) return static_cast<unsigned>(v.GetUint64());
    if (v.IsDouble()) return static_cast<unsigned>(v.GetDouble());
    throw std::runtime_error("JSON value is not an unsigned integer (unsigned)");
}

template <>
inline int64_t ReadJsonAs<int64_t>(const rapidjson::Value& v)
{
    if (v.IsInt64()) return v.GetInt64();
    if (v.IsInt()) return static_cast<int64_t>(v.GetInt());
    if (v.IsDouble()) return static_cast<int64_t>(v.GetDouble());
    throw std::runtime_error("JSON value is not an integer (int64_t)");
}

template <>
inline uint64_t ReadJsonAs<uint64_t>(const rapidjson::Value& v)
{
    if (v.IsUint64()) return v.GetUint64();
    if (v.IsUint()) return static_cast<uint64_t>(v.GetUint());
    if (v.IsDouble()) return static_cast<uint64_t>(v.GetDouble());
    throw std::runtime_error("JSON value is not an unsigned integer (uint64_t)");
}

template <>
inline double ReadJsonAs<double>(const rapidjson::Value& v)
{
    if (v.IsDouble()) return v.GetDouble();
    if (v.IsInt()) return static_cast<double>(v.GetInt());
    if (v.IsInt64()) return static_cast<double>(v.GetInt64());
    throw std::runtime_error("JSON value is not a number (double)");
}

template <>
inline float ReadJsonAs<float>(const rapidjson::Value& v)
{
    return static_cast<float>(ReadJsonAs<double>(v));
}

template <>
inline std::string ReadJsonAs<std::string>(const rapidjson::Value& v)
{
    if (!v.IsString()) throw std::runtime_error("JSON value is not a string");
    return std::string(v.GetString(), v.GetStringLength());
}

#pragma region Macros

// TypeDescriptor for primitive types
// Supports the same primitive types as originally.
// CHANGE: Deserialize uses the ReadJsonAs<T>() helper instead of value["data"].Get<T>().
#define TYPE_DESCRIPTOR(NAME, TYPE) \
  struct ENGINE_API TypeDescriptor_##NAME : TypeDescriptor \
  { \
    TypeDescriptor_##NAME() : TypeDescriptor{ #TYPE, sizeof(TYPE) } {} \
    virtual void Dump(const void* obj, std::ostream& os, int) const override \
    { \
      os << #TYPE << "{" << *(const TYPE*)obj << "}"; \
    } \
    virtual void Serialize(const void* obj, std::ostream& os) const override \
    { \
      /* CHANGE: maintain textual JSON semantics as before */ \
      os << R"({"type":")" << #TYPE << R"(","data":)" << *(const TYPE*)obj << "}"; \
    } \
    virtual void Deserialize(void* obj, const json& value) const override \
    { \
      /* CHANGE: use ReadJsonAs to obtain the correct type from rapidjson */ \
      if (!value.IsObject() || !value.HasMember("data")) throw std::runtime_error("Malformed JSON for primitive type"); \
      TYPE data = ReadJsonAs<TYPE>(value["data"]); \
      *(TYPE*)obj = data; \
    } \
  }; \
  template <> \
  ENGINE_API TypeDescriptor* GetPrimitiveDescriptor<TYPE>() \
  { \
    static TypeDescriptor_##NAME type_desc; \
    std::lock_guard<std::mutex> lock(TypeDescriptor::descriptor_registry_mutex()); /* CHANGE: thread-safe registration */ \
    if (TYPE_DESCRIPTOR_LOOKUP.count(type_desc.name) == 0) { TYPE_DESCRIPTOR_LOOKUP[type_desc.name] = &type_desc; } \
    return &type_desc; \
  }

#pragma endregion

#pragma region Primitive descriptors

// NOTE: We do NOT re-register TypeDescriptor itself here. The earlier code attempted to register metadata for
// the TypeDescriptor struct as a reflected type; that requires the TypeDescriptor definition to contain
// the reflection macro. In the Android-ported header we omitted that to avoid polluting core runtime types.

TYPE_DESCRIPTOR(Int, int)
TYPE_DESCRIPTOR(Unsigned, unsigned)
TYPE_DESCRIPTOR(LongLong, int64_t)
TYPE_DESCRIPTOR(UnsignedLongLong, uint64_t)
TYPE_DESCRIPTOR(Double, double)
TYPE_DESCRIPTOR(Float, float)

// bool specialization — preserves original JSON true/false semantics
struct ENGINE_API TypeDescriptor_Bool : TypeDescriptor
{
    TypeDescriptor_Bool() : TypeDescriptor{ "bool", sizeof(bool) } {}
    virtual void Dump(const void* obj, std::ostream& os, int) const override { os << "bool{" << *(const bool*)obj << "}"; }
    virtual void Serialize(const void* obj, std::ostream& os) const override { os << R"({"type":")" << "bool" << R"(","data":)" << ((*(const bool*)obj) ? "true" : "false") << "}"; }
    virtual void Deserialize(void* obj, const json& value) const override
    {
        if (!value.IsObject() || !value.HasMember("data")) throw std::runtime_error("Malformed JSON for bool");
        bool data = ReadJsonAs<bool>(value["data"]);
        *(bool*)obj = data;
    }
};
template <>
ENGINE_API TypeDescriptor* GetPrimitiveDescriptor<bool>()
{
    static TypeDescriptor_Bool type_desc;
    std::lock_guard<std::mutex> lock(TypeDescriptor::descriptor_registry_mutex());
    if (TYPE_DESCRIPTOR_LOOKUP.count(type_desc.name) == 0) TYPE_DESCRIPTOR_LOOKUP[type_desc.name] = &type_desc;
    return &type_desc;
}

// std::string specialization
struct ENGINE_API TypeDescriptor_StdString : TypeDescriptor
{
    TypeDescriptor_StdString() : TypeDescriptor{ "std::string", sizeof(std::string) } {}

    virtual void Dump(const void* obj, std::ostream& os, int) const override
    {
        os << "std::string{" << *(const std::string*)obj << "}";
    }

    virtual void Serialize(const void* obj, std::ostream& os) const override
    {
        const std::string& data = *(const std::string*)obj;
        // CHANGE: reuse the original escaping logic but note: this is a simplistic escape suitable for ASCII/Basic BMP.
        std::ostringstream escaped;
        for (unsigned char c : data)
        {
            switch (c)
            {
            case '\\': escaped << "\\\\"; break;
            case '"': escaped << "\\\""; break;
            case '\b': escaped << "\\b"; break;
            case '\f': escaped << "\\f"; break;
            case '\n': escaped << "\\n"; break;
            case '\r': escaped << "\\r"; break;
            case '\t': escaped << "\\t"; break;
            default:
                if (c < 0x20 || c > 0x7E)
                {
                    escaped << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c) << std::dec << std::setfill(' ');
                }
                else escaped << c;
                break;
            }
        }
        os << R"({"type":")" << "std::string" << R"(","data":")" << escaped.str() << R"("})";
    }

    virtual void Deserialize(void* obj, const json& value) const override
    {
        if (!value.IsObject() || !value.HasMember("data") || !value["data"].IsString()) throw std::runtime_error("Malformed JSON for std::string");
        // CHANGE: rapidjson already gives us a properly unescaped string via GetString(); prefer that to custom unescaping.
        std::string s = ReadJsonAs<std::string>(value["data"]);
        *(std::string*)obj = s;
    }
};

template <>
ENGINE_API TypeDescriptor* GetPrimitiveDescriptor<std::string>()
{
    static TypeDescriptor_StdString type_desc;
    std::lock_guard<std::mutex> lock(TypeDescriptor::descriptor_registry_mutex());
    if (TYPE_DESCRIPTOR_LOOKUP.count(type_desc.name) == 0) TYPE_DESCRIPTOR_LOOKUP[type_desc.name] = &type_desc;
    return &type_desc;
}

#pragma endregion
